
#ifndef DOMPASCH_MALLOB_OPTIONS_LIST_HPP
#define DOMPASCH_MALLOB_OPTIONS_LIST_HPP

#include "util/option.hpp"
#include "util/hashing.hpp"

// All declared options will be stored in this member of the Parameters class.
OptMap _map;

// New options can be added here. They will then be initialized as member fields in the Parameters class.
// The value of an option can be queried on a Parameters object _params as such:
// _params.optionmember()

//  TYPE  member name                    option ID (short, long)            default (, min, max)        description

OPT_BOOL(addOldLglDiversifiers,          "aold", "",                        false,                      "Add additional old diversifiers to Lingeling")
OPT_BOOL(collectClauseHistory,           "ch", "",                          false,                      "Employ clause history collection mechanism")
OPT_BOOL(coloredOutput,                  "colors", "",                      false,                      "Colored terminal output based on messages' verbosity")
OPT_BOOL(continuousGrowth,               "cg", "",                          true,                       "Continuous growth of job demands")
OPT_BOOL(delayMonkey,                    "delaymonkey", "",                 false,                      "Small chance for each MPI call to block for some random amount of time")
OPT_BOOL(derandomize,                    "derandomize", "",                 true,                       "Derandomize job bouncing and build a <bounce-alternatives>-regular message graph instead")
OPT_BOOL(help,                           "h", "help",                       false,                      "Print help and exit")
OPT_BOOL(jitterJobPriorities,            "jjp", "jitter-job-priorities",    true,                       "Jitter job priorities to break ties during rebalancing")
OPT_BOOL(latencyMonkey,                  "latencymonkey", "",               false,                      "Block all MPI_Isend operations by a small randomized amount of time")
OPT_BOOL(logToFiles,                     "filelog", "",                     true,                       "Logging to filesystem")
OPT_BOOL(monitorMpi,                     "mmpi", "monitor-mpi",             false,                      "Launch an additional thread per process checking when the main thread is inside an MPI call")
OPT_BOOL(phaseDiversification,           "phasediv", "",                    true,                       "Diversify solvers based on phase in addition to native diversification")
OPT_BOOL(quiet,                          "q", "quiet",                      false,                      "Do not log to stdout besides critical information")
OPT_BOOL(shuffleSharedClauses,           "shufshcls", "",                   false,                      "Shuffle literals in each shared clause randomly on import")
OPT_BOOL(useChecksums,                   "checksums", "",                   false,                      "Compute and verify checksum for every job description transfer")
OPT_BOOL(warmup,                         "warmup", "",                      false,                      "Do one explicit All-To-All warmup among all nodes in the beginning")
OPT_BOOL(workRequests,                   "wr", "",                          false,                      "Send around work requests similarly to job requests")
OPT_BOOL(yield,                          "yield", "",                       false,                      "Yield manager thread whenever there are no new messages")
OPT_BOOL(zeroOnlyLogging,                "0o", "",                          false,                      "Only PE of rank zero does logging")

OPT_INT(activeJobsPerClient,             "ajpc", "lbc",                     0,    0, LARGE_INT,         "Make each client have up to this many active jobs at any given time")
OPT_INT(clauseBufferBaseSize,            "cbbs", "",                        1500, 0, MAX_INT,           "Clause buffer base size in integers")
OPT_INT(clauseHistoryAggregationFactor,  "chaf", "",                        5,    1, LARGE_INT,         "Aggregate historic clause batches by this factor")
OPT_INT(clauseHistoryShortTermMemSize,   "chstms", "",                      60,   1, LARGE_INT,         "Save this many \"full\" aggregated epochs until reducing them")
OPT_INT(finalHardLbdLimit,               "fhlbd", "",                       LARGE_INT, 0, LARGE_INT,    "After max. number of clause prod. increases, this MUST be fulfilled for any clause to be shared")
OPT_INT(finalSoftLbdLimit,               "fslbd", "",                       LARGE_INT, 0, LARGE_INT,    "After max. number of clause prod. increases, this must be fulfilled for any clause to be shared except for special cases")
OPT_INT(firstApiIndex,                   "fapii", "",                       0,    0, LARGE_INT,         "1st API index: with c clients, uses .api/jobs.{<index>..<index>+c-1}/ as directories")
OPT_INT(hopsBetweenBfs,                  "hbbfs", "",                       10,   0, MAX_INT,           "After a job request hopped this many times after unsuccessful \"hill climbing\" BFS, perform another BFS")
OPT_INT(hardMaxClauseLength,             "hmcl", "",                        30,   0, LARGE_INT,         "Only share clauses up to this length")
OPT_INT(hopsUntilBfs,                    "hubfs", "",                       100,  0, MAX_INT,           "After a job request hopped this many times, perform a \"hill climbing\" BFS")
OPT_INT(initialHardLbdLimit,             "ihlbd", "",                       LARGE_INT, 0, LARGE_INT,    "Before any clause prod. increase, this MUST be fulfilled for any clause to be shared")
OPT_INT(initialSoftLbdLimit,             "islbd", "",                       LARGE_INT, 0, LARGE_INT,    "Before any clause prod. increase, this must be fulfilled for any clause to be shared except for special cases")
OPT_INT(jobCacheSize,                    "jc", "job-cache",                 4,    0, LARGE_INT,         "Size of job cache per PE for suspended yet unfinished job nodes")
OPT_INT(numBounceAlternatives,           "ba", "",                          4,    1, LARGE_INT,         "Number of bounce alternatives per PE (only relevant if -derandomize)")
OPT_INT(maxBfsDepth,                     "mbfsd", "",                       4,    0, LARGE_INT,         "Max. depth to explore with hill climbing BFS for job requests")
OPT_INT(maxDemand,                       "md", "max-demand",                0,    0, LARGE_INT,         "Limit any job's demand to this value")
OPT_INT(maxIdleDistance,                 "mid", "",                         0,    0, LARGE_INT,         "Propagate idle distance of workers up to this limit through worker graph to weight randomness in request bouncing")
OPT_INT(maxLbdPartitioningSize,          "mlbdps", "",                      8,    1, LARGE_INT,         "Store clauses with up to this LBD in separate buckets")
OPT_INT(numClients,                      "c", "num-clients",                1,    0, LARGE_INT,         "Number of client nodes")
OPT_INT(numJobs,                         "J", "num-jobs",                   0,    0, LARGE_INT,         "Exit as soon as this number of jobs has been processed")
OPT_INT(numThreadsPerProcess,            "t", "threads-per-process",        1,    0, LARGE_INT,         "Number of worker threads per node")
OPT_INT(sizeLimitPerProcess,             "slpp", "size-limit-per-process",  0,    0, MAX_INT,           "No more than max(1, floor(<limit>/<jobsize>)) threads are spawned per process")
OPT_INT(sleepMicrosecs,                  "sleep", "",                       100,  0, LARGE_INT,         "Sleep this many microseconds between loop cycles of worker main thread")
OPT_INT(softMaxClauseLength,             "hmcl", "",                        30,   0, LARGE_INT,         "Only share clauses up to this length except for special cases")
OPT_INT(verbosity,                       "v", "verbosity",                  2,    0, 6,                 "Logging verbosity: 0=CRIT 1=WARN 2=INFO 3=VERB 4=VVERB 5=DEBG")

OPT_FLOAT(appCommPeriod,                 "s", "app-comm-period",            1,    0, LARGE_INT,         "Do job-internal communication every t seconds") 
OPT_FLOAT(balancingPeriod,               "p", "balancing-period",           0.1,  0, LARGE_INT,         "Minimum interval between subsequent rounds of balancing")
OPT_FLOAT(clauseBufferDiscountFactor,    "cbdf", "",                        0.9,  0.5, 1,               "Clause buffer discount factor: reduce buffer size per PE by <factor> each depth")
OPT_FLOAT(clauseFilterHalfLife,          "cfhl", "",                        0,    0, LARGE_INT,         "Set clause filter half life of clauses until forgotten")
OPT_FLOAT(growthPeriod,                  "g", "growth-period",              0,    0, LARGE_INT,         "Grow job demand exponentially every t seconds (0: immediate full growth)" )
OPT_FLOAT(increaseClauseProductionRatio, "icpr", "",                        0,    0, 1,                 "Increase a solver's Clause Production when it fills less than <ratio> of its buffer")
OPT_FLOAT(inputShuffleProbability,       "isp", "",                         0,    0, 1,                 "Probability for solver with exhausted diversification to shuffle all clauses and all literals of each clause in the input")
OPT_FLOAT(jobCommUpdatePeriod,           "jcup", "",                        0,    0, LARGE_INT,         "Job communicator update period (0: never update)" )
OPT_FLOAT(jobCpuLimit,                   "jcl", "job-cpu-limit",            0,    0, LARGE_INT,         "Timeout an instance after x cpu seconds")
OPT_FLOAT(jobWallclockLimit,             "jwl", "job-wallclock-limit",      0,    0, LARGE_INT,         "Timeout an instance after x seconds wall clock time")
OPT_FLOAT(loadFactor,                    "l", "load-factor",                1,    0, 1,                 "Load factor to be aimed at")
OPT_FLOAT(requestTimeout,                "rto", "request-timeout",          0,    0, LARGE_INT,         "Request timeout: discard non-root job requests when older than this many seconds")
OPT_FLOAT(timeLimit,                     "T", "time-limit",                 0,    0, LARGE_INT,         "Run entire system for at most this many seconds")

OPT_STRING(applicationSpawnMode,         "appmode", "",                     "fork",                     "Application mode: \"fork\" (spawn child process for each job on each MPI process) or \"thread\" (execute jobs in separate threads but within the same process)")
OPT_STRING(balanceRoundingMode,          "r",  "",                          "bisec",                    "Mode of rounding of assignments in balancing (\"prob\": probabilistic, \"bisec\": iterative bisection, \"floor\" - always round down)")
OPT_STRING(hordeConfig,                  "hConf", "",                       "",                         "Supply Horde config for solver subprocess [internal option, do not use]")
OPT_STRING(logDirectory,                 "log", "log-directory",            "",                         "Directory to save logs in")
OPT_STRING(monoFilename,                 "mono", "",                        "",                         "Mono instance: Solve the provided CNF instance with full power, then exit")
OPT_STRING(satSolverSequence,            "satsolver",  "",                  "mcL",                      "Sequence of SAT solvers to cycle through for each job, one character per solver (capital letter for true incremental solver, lowercase for pseudo-incremental solving): l=lingeling c=cadical g=glucose m=mergesat")
OPT_STRING(solutionToFile,               "s2f", "solution-to-file",         "",                         "Write solutions to file with provided base name + job ID")

#endif
