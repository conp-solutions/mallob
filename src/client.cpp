
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <list>

#include "client.hpp"
#include "util/sys/timer.hpp"
#include "util/logger.hpp"
#include "util/permutation.hpp"
#include "util/sys/proc.hpp"
#include "data/job_transfer.hpp"
#include "data/job_result.hpp"
#include "util/random.hpp"
#include "app/sat/sat_constants.h"
#include "util/sys/terminator.hpp"
#include "data/job_reader.hpp"

// Executed by a separate worker thread
void Client::readIncomingJobs(Logger log) {

    log.log(V3_VERB, "Starting\n");

    std::list<std::pair<std::thread, std::atomic_bool*>> readerTasks;
    std::atomic_int numActiveTasks = 0;

    while (!checkTerminate()) {

        float time = Timer::elapsedSeconds();

        if (_num_incoming_jobs > 0 && numActiveTasks+_num_loaded_jobs < 32) {
            auto lock = _incoming_job_lock.getLock();

            JobMetadata foundJob;
            for (auto& data : _incoming_job_queue) {
                
                // Jobs are sorted by arrival:
                // If this job has not arrived yet, then none have arrived yet
                if (time < data.description->getArrival()) break;

                // Check job's dependencies
                bool dependenciesSatisfied = true;
                {
                    auto lock = _done_job_lock.getLock();
                    for (int jobId : data.dependencies) {
                        if (!_done_jobs.count(jobId)) {
                            dependenciesSatisfied = false;
                            break;
                        }
                    }
                    if (data.description->isIncremental() && data.description->getRevision() > 0) {
                        // Check if the precursor of this incremental job is already done
                        if (!_done_jobs.count(data.description->getId()))
                            dependenciesSatisfied = false; // no job with this ID is done yet
                        else if (data.description->getRevision() != _done_jobs[data.description->getId()].revision+1)
                            dependenciesSatisfied = false; // job with correct revision not done yet
                        else {
                            data.description->setChecksum(_done_jobs[data.description->getId()].lastChecksum);
                        }
                    }
                }
                if (!dependenciesSatisfied) continue;

                // Job can be read
                foundJob = data;
                auto finishedFlag = new std::atomic_bool(false);
                readerTasks.emplace_back([this, &log, foundJob, finishedFlag]() {

                    // Read job
                    int id = foundJob.description->getId();
                    float time = Timer::elapsedSeconds();
                    log.log(V3_VERB, "[T] Reading job #%i rev. %i (%s) ...\n", id, foundJob.description->getRevision(), foundJob.file.c_str());
                    bool success = JobReader::read(foundJob.file, *foundJob.description);
                    if (!success) {
                        log.log(V1_WARN, "[T] File %s could not be opened - skipping #%i\n", foundJob.file.c_str(), id);
                    } else {
                        time = Timer::elapsedSeconds() - time;
                        log.log(V3_VERB, "[T] Initialized job #%i (%s) in %.3fs: %ld lits w/ separators, %ld assumptions\n", 
                                id, foundJob.file.c_str(), time, foundJob.description->getNumFormulaLiterals(), 
                                foundJob.description->getNumAssumptionLiterals());
                        
                        // Enqueue in ready jobs
                        auto lock = _ready_job_lock.getLock();
                        _ready_job_queue.emplace_back(foundJob.description);
                        _num_ready_jobs++;
                    }
                    *finishedFlag = true;

                }, finishedFlag);
                numActiveTasks++;
                break;
            }

            // If a job was found, delete it from incoming queue
            if (foundJob.description) {
                _incoming_job_queue.erase(foundJob);
                _num_incoming_jobs--;
            }
        }

        // Check jobs being read for completion
        for (auto it = readerTasks.begin(); it != readerTasks.end(); ++it) {
            auto& [thread, flag] = *it;

            assert(thread.joinable());
            
            if (*flag) {
                // Job reading done -- clean up
                thread.join();
                delete flag;
                it = readerTasks.erase(it);
                numActiveTasks--;
                _num_loaded_jobs++;
                --it;
                _sys_state.addLocal(SYSSTATE_PARSED_JOBS, 1);
            }
        }

        // Rest for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    log.log(V3_VERB, "Stopping\n");
    log.flush();

    for (auto& [thread, flag] : readerTasks) thread.join();
}

void Client::handleNewJob(JobMetadata&& data) {

    if (data.done) {
        // Incremental job notified to be finished
        int jobId = data.description->getId();
        auto lock = _done_job_lock.getLock();
        _recently_done_jobs.insert(jobId);
        return;
    }

    // Introduce new job into "incoming" queue
    {
        auto lock = _incoming_job_lock.getLock();
        _incoming_job_queue.insert(std::move(data));
    }
    _num_incoming_jobs++;
    _sys_state.addLocal(SYSSTATE_ENTERED_JOBS, 1);
}

void Client::init() {

    int internalRank = MyMpi::rank(_comm) + _params.firstApiIndex();

    _file_adapter = std::unique_ptr<JobFileAdapter>(
        new JobFileAdapter(internalRank, _params, 
            Logger::getMainInstance().copy("API", "#0."),
            ".api/jobs." + std::to_string(internalRank) + "/", 
            [&](JobMetadata&& data) {handleNewJob(std::move(data));}
        )
    );
    _instance_reader_thread = std::thread([this]() {
        readIncomingJobs(
            Logger::getMainInstance().copy("<Reader>", "#-1.")
        );
    });
    log(V2_INFO, "Client main thread started\n");

    // Begin listening to incoming messages
    MyMpi::beginListening();

    log(V4_VVER, "Global init barrier ...\n");
    MPI_Barrier(MPI_COMM_WORLD);
    log(V4_VVER, "Passed global init barrier\n");
}

bool Client::checkTerminate() {

    if (Terminator::isTerminating()) {
        log(V2_INFO, "Terminating.\n");
        return true;
    }
    if (Timer::globalTimelimReached(_params)) {
        log(V2_INFO, "Global timeout: terminating\n");
        Terminator::setTerminating();
        return true;
    }
    return false;
}

void Client::mainProgram() {

    float lastStatTime = Timer::elapsedSeconds();
    std::vector<int> finishedHandleIds;

    while (!checkTerminate()) {

        float time = Timer::elapsedSeconds();

        // Print memory usage info
        if (time - lastStatTime > 5) {
            auto info = Proc::getRuntimeInfo(Proc::getPid(), Proc::SubprocessMode::FLAT);
            info.vmUsage *= 0.001 * 0.001;
            info.residentSetSize *= 0.001 * 0.001;
            log(V4_VVER, "mainthread_cpu=%i\n", info.cpu);
            log(V3_VERB, "mem=%.2fGB\n", info.residentSetSize);
            lastStatTime = time;
        }

        // Send notification messages for recently done jobs
        robin_hood::unordered_flat_set<int, robin_hood::hash<int>> doneJobs;
        {
            auto lock = _done_job_lock.getLock();
            doneJobs = std::move(_recently_done_jobs);
            _recently_done_jobs.clear();
        }
        for (int jobId : doneJobs) {
            log(LOG_ADD_DESTRANK | V3_VERB, "Notifying #%i:0 that job is done", _root_nodes[jobId], jobId);
            IntVec payload({jobId});
            MyMpi::isend(MPI_COMM_WORLD, _root_nodes[jobId], MSG_INCREMENTAL_JOB_FINISHED, payload);
            finishJob(jobId, /*hasIncrementalSuccessors=*/false);
        }

        // Introduce next job(s) as applicable
        // (only one job at a time to react better
        // to outside events without too much latency)
        introduceNextJob();

        // Poll messages, if present
        auto maybeHandle = MyMpi::poll(time);
        if (maybeHandle) {
            // Process message
            auto& handle = *maybeHandle;
            log(LOG_ADD_SRCRANK | V5_DEBG, "process msg tag=%i", handle.source, handle.tag);

            if (handle.tag == MSG_NOTIFY_JOB_DONE) {
                handleJobDone(handle);
            } else if (handle.tag == MSG_SEND_JOB_RESULT) {
                handleSendJobResult(handle);
            } else if (handle.tag == MSG_NOTIFY_JOB_ABORTING) {
                handleAbort(handle);
            } else if (handle.tag == MSG_OFFER_ADOPTION) {
                handleRequestBecomeChild(handle);
            } else if (handle.tag == MSG_CONFIRM_ADOPTION) {
                handleAckAcceptBecomeChild(handle);
            } else if (handle.tag == MSG_DO_EXIT) {
                handleExit(handle);
            } else {
                log(LOG_ADD_SRCRANK | V1_WARN, "Unknown msg tag %i", handle.source, handle.tag);
            }
        }

        MyMpi::testSentHandles(&finishedHandleIds);
        // Clear job descriptions which are sent
        for (int id : finishedHandleIds) {
            if (_transfer_msg_id_to_job_id.count(id)) {
                int jobId = _transfer_msg_id_to_job_id.at(id);
                if (_active_jobs.count(jobId)) {
                    log(V3_VERB, "Clear description of sent job #%i\n", jobId);
                    _active_jobs.at(jobId)->clearPayload();
                    _num_loaded_jobs--;
                }
                _transfer_msg_id_to_job_id.erase(id);
            }
        }
        finishedHandleIds.clear();
        
        // Advance an all-reduction of the current system state
        if (_sys_state.aggregate(time)) {
            float* result = _sys_state.getGlobal();
            int processed = (int)result[SYSSTATE_PROCESSED_JOBS];
            int verb = (MyMpi::rank(_comm) == 0 ? V2_INFO : V5_DEBG);
            log(verb, "sysstate entered=%i parsed=%i scheduled=%i processed=%i\n", 
                        (int)result[SYSSTATE_ENTERED_JOBS], 
                        (int)result[SYSSTATE_PARSED_JOBS], 
                        (int)result[SYSSTATE_SCHEDULED_JOBS], 
                        processed);
            int jobLimit = _params.numJobs();
            if (jobLimit > 0 && processed >= jobLimit) {
                log(V2_INFO, "Job limit reached.\n");
                // Job limit reached - exit
                Terminator::setTerminating();
                // Send MSG_EXIT to worker of rank 0, which will broadcast it
                MyMpi::isend(MPI_COMM_WORLD, 0, MSG_DO_EXIT, IntVec({0}));
            }
        }

        // Sleep for a bit
        usleep(1000); // 1000 = 1 millisecond
    }

    Logger::getMainInstance().flush();
    fflush(stdout);
}

int Client::getMaxNumParallelJobs() {
    return _params.activeJobsPerClient();
}

void Client::introduceNextJob() {

    if (checkTerminate()) return;

    // Are there any non-introduced jobs left?
    if (_num_ready_jobs == 0) return;
    
    // Check if there is space for another active job in this client's "bucket"
    size_t lbc = getMaxNumParallelJobs();
    if (lbc > 0 && _active_jobs.size() >= lbc) return;

    // Remove first job from ready queue
    std::shared_ptr<JobDescription> jobPtr;
    {
        auto lock = _ready_job_lock.getLock();
        jobPtr = _ready_job_queue.front();
        _ready_job_queue.pop_front();
    }
    _num_ready_jobs--;

    // Store as an active job
    JobDescription& job = *jobPtr;
    int jobId = job.getId();
    _active_jobs[jobId] = jobPtr;
    _sys_state.addLocal(SYSSTATE_SCHEDULED_JOBS, 1);

    // Set actual job arrival
    float time = Timer::elapsedSeconds();
    job.setArrival(time);

    int nodeRank;
    if (job.isIncremental()) {
        // Incremental job: Send request to root node in standby
        nodeRank = _root_nodes[jobId];
    } else {
        // Find the job's canonical initial node
        int n = MyMpi::size(MPI_COMM_WORLD) - MyMpi::size(_comm);
        log(V4_VVER, "Creating permutation of size %i ...\n", n);
        AdjustablePermutation p(n, jobId);
        nodeRank = p.get(0);
    }   
    log(V4_VVER, "%i\n", job.getRevision());

    JobRequest req(jobId, job.getApplication(), /*rootRank=*/-1, /*requestingNodeRank=*/_world_rank, 
        /*requestedNodeIndex=*/0, /*timeOfBirth=*/time, /*balancingEpoch=*/-1, /*numHops=*/0);
    req.currentRevision = job.getRevision();
    req.timeOfBirth = job.getArrival();

    log(LOG_ADD_DESTRANK | V2_INFO, "Introducing job #%i : %s", nodeRank, jobId, req.toStr().c_str());
    MyMpi::isend(MPI_COMM_WORLD, nodeRank, MSG_REQUEST_NODE, req);
}

void Client::handleRequestBecomeChild(MessageHandle& handle) {
    JobRequest req = Serializable::get<JobRequest>(handle.getRecvData());
    const JobDescription& desc = *_active_jobs[req.jobId];
    log(V4_VVER, "OFFER for %s\n", req.toStr().c_str());

    // Send job signature
    int firstIncludedRevision = req.lastKnownRevision+1;
    JobSignature sig(req.jobId, /*rootRank=*/handle.source, firstIncludedRevision, 
        firstIncludedRevision <= req.currentRevision ? desc.getTransferSize(firstIncludedRevision) : 0);
    log(LOG_ADD_DESTRANK | V4_VVER, "Sending job signature of #%i rev. %i: transfer size %i", handle.source, req.jobId, req.currentRevision, sig.transferSize);
    MyMpi::isend(MPI_COMM_WORLD, handle.source, MSG_ACCEPT_ADOPTION_OFFER, sig);
    //stats.increment("sentMessages");
}

void Client::handleAckAcceptBecomeChild(MessageHandle& handle) {
    JobRequest req = Serializable::get<JobRequest>(handle.getRecvData());
    JobDescription& desc = *_active_jobs[req.jobId];
    assert(desc.getId() == req.jobId || log_return_false("%i != %i\n", desc.getId(), req.jobId));
    log(LOG_ADD_DESTRANK | V4_VVER, "Sending job desc. of #%i rev. %i of size %i", handle.source, desc.getId(), desc.getRevision(), desc.getFullTransferSize());
    _root_nodes[req.jobId] = handle.source;
    auto data = desc.getSerialization();

    int jobId = Serializable::get<int>(*data);    
    int msgId = MyMpi::isend(MPI_COMM_WORLD, handle.source, MSG_SEND_JOB_DESCRIPTION, data);
    log(LOG_ADD_DESTRANK | V4_VVER, "Sent job desc. of #%i of size %i", handle.source, jobId, data->size());
    _transfer_msg_id_to_job_id[msgId] = jobId;
}

void Client::handleJobDone(MessageHandle& handle) {
    IntPair recv = Serializable::get<IntPair>(handle.getRecvData());
    int jobId = recv.first;
    int resultSize = recv.second;
    log(LOG_ADD_SRCRANK | V4_VVER, "Will receive job result, length %i, for job #%i", handle.source, resultSize, jobId);
    MyMpi::isend(MPI_COMM_WORLD, handle.source, MSG_QUERY_JOB_RESULT, handle.getRecvData());
    MyMpi::irecv(MPI_COMM_WORLD, handle.source, MSG_SEND_JOB_RESULT, resultSize);
}

void Client::handleSendJobResult(MessageHandle& handle) {

    JobResult jobResult = Serializable::get<JobResult>(handle.getRecvData());
    int jobId = jobResult.id;
    int resultCode = jobResult.result;
    int revision = jobResult.revision;

    log(LOG_ADD_SRCRANK | V4_VVER, "Received result of job #%i rev. %i, code: %i", handle.source, jobId, revision, resultCode);
    JobDescription& desc = *_active_jobs[jobId];

    // Output response time and solution header
    log(V2_INFO, "RESPONSE_TIME #%i %.6f rev. %i\n", jobId, Timer::elapsedSeconds() - desc.getArrival(), revision);
    log(V2_INFO, "SOLUTION #%i %s rev. %i\n", jobId, resultCode == RESULT_SAT ? "SAT" : "UNSAT", revision);

    // Write full solution to file, if desired
    std::string baseFilename = _params.solutionToFile();
    if (!baseFilename.empty()) {
        std::string filename = baseFilename + "_" + std::to_string(jobId) + "." + std::to_string(revision);
        std::ofstream file;
        file.open(filename);
        if (!file.is_open()) {
            log(V0_CRIT, "ERROR: Could not open solution file\n");
        } else {
            file << "c SOLUTION #" << jobId << " rev. " << revision << " ";
            file << (resultCode == RESULT_SAT ? "SAT" : resultCode == RESULT_UNSAT ? "UNSAT" : "UNKNOWN") << "\n"; 
            for (auto lit : jobResult.solution) {
                if (lit == 0) continue;
                file << lit << " ";
            }
            file << "\n";
            file.close();
        }
    }

    if (_file_adapter) {
        _file_adapter->handleJobDone(jobResult);
    }

    finishJob(jobId, /*hasIncrementalSuccessors=*/_active_jobs[jobId]->isIncremental());
}

void Client::handleAbort(MessageHandle& handle) {

    IntVec request = Serializable::get<IntVec>(handle.getRecvData());
    int jobId = request[0];
    log(LOG_ADD_SRCRANK | V2_INFO, "TIMEOUT #%i %.6f", handle.source, jobId, 
            Timer::elapsedSeconds() - _active_jobs[jobId]->getArrival());
    
    if (_file_adapter) {
        JobResult result;
        result.id = jobId;
        result.revision = _active_jobs[result.id]->getRevision();
        result.result = 0;
        _file_adapter->handleJobDone(result);
    }

    finishJob(jobId, /*hasIncrementalSuccessors=*/_active_jobs[jobId]->isIncremental());
}

void Client::finishJob(int jobId, bool hasIncrementalSuccessors) {

    // Clean up job, remember as done
    {
        auto lock = _done_job_lock.getLock();
        _done_jobs[jobId] = DoneInfo{_active_jobs[jobId]->getRevision(), _active_jobs[jobId]->getChecksum()};
    }
    if (!hasIncrementalSuccessors) {
        _root_nodes.erase(jobId);
        _active_jobs.erase(jobId);
        _sys_state.addLocal(SYSSTATE_PROCESSED_JOBS, 1);
    }

    introduceNextJob();
}

void Client::handleExit(MessageHandle& handle) {
    Terminator::setTerminating();
}

Client::~Client() {
    if (_instance_reader_thread.joinable())
        _instance_reader_thread.join();

    _file_adapter.reset();

    // Merge logs from instance reader
    Logger::getMainInstance().mergeJobLogs(0);
    Logger::getMainInstance().mergeJobLogs(-1);

    log(V4_VVER, "Leaving client destructor\n");
}