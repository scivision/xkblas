# include "demangle.hpp"
# include "intervals.hpp"
# include "dependency-tree.hpp"
# include "task.hpp"

# include <cmath>
# include <cstdlib>

# define MAX_TASKS 1000000

static inline uint64_t
get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

static int NINSERT = 0;

template<int K>
static inline KTask<K> *
get_tasks(void)
{
    static KTask<K> tasks[MAX_TASKS];
    return tasks;
}

template<int K>
static inline int &
get_ntasks()
{
    static int ntasks = 0;
    return ntasks;
}

template<int K>
static inline int
get_nedges(void)
{
    KTask<K> * tasks = get_tasks<K>();
    int ntasks = get_ntasks<K>();
    int nedges = 0;
    for (int i = 0 ; i < ntasks ; ++i)
    {
        KTask<K> * task = tasks + i;
        nedges += task->edges.size();
    }
    return nedges;
}

template<int K>
KTask<K> *
task_new(void)
{
    KTask<K> * tasks = get_tasks<K>();
    int & ntasks = get_ntasks<K>();

    KTask<K> * task = &(tasks[ntasks++]);
    if (ntasks > MAX_TASKS)
    {
        fprintf(stderr, "Increase 'MAX_TASKS' in %s:%d\n", __FILE__, __LINE__);
        exit(0);
    }
    return task;
}

// Insert a region into the tree
template<int K>
static void
insert(
    KDependencyTree<K> & tree,
    access_mode_t mode,
    Interval intervals[K]
) {
    Intervals<K> region(intervals);
//    std::cout << "inserting " << (mode == ACCESS_MODE_RW ? "rw" : mode == ACCESS_MODE_R ? "r" : mode == ACCESS_MODE_W ? "w" :  "unk") << " " << region << std::endl;
    KTask<K> * task = task_new<K>();
    tree.intersect(task, region, mode);
    tree.insert(task, region, mode);
    ++NINSERT;
}

//Generate 'n' disjoint hyperplans
template<int K, int PLAN_DIM>
void
disjoint_hyperplans(KDependencyTree<K> & tree, int n)
{
    static_assert(0 <= PLAN_DIM && PLAN_DIM < K);

    for (int i = 0 ; i < n ; ++i)
    {
        Interval intervals[K];
        for (int k = 0 ; k < K ; ++k)
        {
            if (k != PLAN_DIM)
            {
                intervals[k].a =   0;
                intervals[k].b = n*8;
            }
            else
            {
                intervals[k].a = 4*(i+0);
                intervals[k].b = 4*(i+1);
            }
        }
        insert(tree, ACCESS_MODE_RW, intervals);
    }
}

//Generate 'n' hypercubes that includes all dimensions but one
template<int K>
void
pyramid(KDependencyTree<K> & tree, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        Interval intervals[K];
        for (int k = 0 ; k < K ; ++k)
        {
            if (k < K - 1)
            {
                intervals[k].a = n*8/2-4*(i+1);
                intervals[k].b = n*8/2+4*(i+1);
            }
            else
            {
                intervals[k].a = 4*(i+0);
                intervals[k].b = 4*(i+1);
            }
        }
        insert(tree, ACCESS_MODE_RW, intervals);
    }
}

//Generate 'n' hypercubes that are included on all dimensions but one
template<int K>
void
pyramid_inverted(KDependencyTree<K> & tree, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        Interval intervals[K];
        for (int k = 0 ; k < K ; ++k)
        {
            if (k < K - 1)
            {
                intervals[k].a = 0+4*i;
                intervals[k].b = n*8-4*i;
            }
            else
            {
                intervals[k].a = 4*(i+0);
                intervals[k].b = 4*(i+1);
            }
        }
        insert(tree, ACCESS_MODE_RW, intervals);
    }
}

// Generate 'n' hypercubes that are successively included into previous ones
template<int K>
static void
squares_included(KDependencyTree<K> & tree, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        Interval intervals[K];
        for (int k = 0 ; k < K ; ++k)
        {
            intervals[k].a =     i*4;
            intervals[k].b = n*8-i*4;
        }
        insert<K>(tree, ACCESS_MODE_RW, intervals);
    }
}

template<int Dimensions, int K, class Callable>
constexpr void meta_for_loop(KDependencyTree<K> & tree, std::array<int, K> & array, int end, Callable & c)
{
    static_assert(Dimensions > 0);
    for(int i = 0; i != end; ++i)
    {
        array[Dimensions-1] = i;
        if constexpr(Dimensions > 1)
            meta_for_loop<Dimensions-1, K>(tree, array, end, c);
        else
            c(tree, array);
    }
}

template<int K, int P>
static void
matrix_tiles_insert(KDependencyTree<K> & tree, std::array<int, K> indices)
{
    Interval intervals[K];
    for (int k = 0 ; k < K ; ++k)
    {
        intervals[k].a = (indices[k]+0)*P;
        intervals[k].b = (indices[k]+1)*P;
    }
    insert(tree, ACCESS_MODE_RW, intervals);
}

// Generate n^K tiles of size P
template<int K, int P>
static void
matrix_tiles(KDependencyTree<K> & tree, int n)
{
    std::array<int, K> array;
    meta_for_loop<K, K>(tree, array, n, matrix_tiles_insert<K, P>);
}

// Test size
static int N = 10;

// Launch tests for a tree of dimension 'K'
template<int K>
static void launch_tests(KDependencyTree<K> & tree)
{
    printf("Running for K=%d and structure %s\n", K, demangle(tree).c_str());

    uint64_t t0 = get_nanotime();
    {
        # if 0
        static_assert(K == 2);

        Interval intervals[] = {
            Interval( 0, 16), Interval( 4,  8),
            Interval( 0, 16), Interval( 8, 12),
            Interval( 0, 16), Interval(12, 16),

            Interval( 2, 14), Interval( 0,  4),
            Interval( 0, 16), Interval( 0,  4),
        };

        for (unsigned int i = 0 ; i < sizeof(intervals) / sizeof(Interval) ; i += 2)
        {
            Interval x[K] = { intervals[i+0], intervals[i+1] };
            insert<K>(tree, ACCESS_MODE_R, x);
        }

        # else

        // hyperplans
        disjoint_hyperplans<K, K-1>(tree, N);
        disjoint_hyperplans<K,   0>(tree, N);

        // matrix test
        int Nth_sqrt = std::pow(N, 1.0/K);
        matrix_tiles<K, 2>(tree, Nth_sqrt/2*2+1);
        matrix_tiles<K, 3>(tree, Nth_sqrt/3*2+1);
        matrix_tiles<K, 5>(tree, Nth_sqrt/5*2+1);
        matrix_tiles<K, 7>(tree, Nth_sqrt/7*2+1);

        // include squares
        squares_included<K>(tree, N);

        // pyramid inverted
        pyramid_inverted<K>(tree, N);

        // pyramid
        pyramid<K>(tree, N);

        # endif
    }
    uint64_t tf = get_nanotime();

    // finish
    double dt = (tf - t0) / 1e9;
    int nelements = tree.size();
    int ntasks = get_ntasks<K>();
    int nedges = get_nedges<K>();
    printf("Took %lf s.\n", dt);
    printf("    Inserted %d regions and %d elements\n", ntasks, nelements);
    printf("        Intervals/s. = %.2lf\n", ntasks / dt);
    printf("         Elements/s. = %.2lf\n", nelements / dt);
    printf("     Inserted %d tasks\n", ntasks);
    printf("        Tasks/s. = %.2lf\n", ntasks / dt);
    printf("    Set %d task edges\n", nedges);
    printf("        Edges/s. = %.2lf\n", nedges / dt);
    printf("\n");
}

// Run for dimension 'K'
template<int K>
static void run(void)
{
    KDependencyTree<K> tree;
    launch_tests(tree);

    # ifdef EXPORT_PDF
    std::cout << "Exporting pdf..." << std::endl;
    tree.export_pdf("dependency");
    # endif /* EXPORT_PDF */
}

int
main(int argc, char ** argv)
{
    if (argc == 2)
        N = atoi(argv[1]);

    printf("Testing with N=%d, you can change running `%s [N]`\n\n", N, argv[0]);

//    run<1>();
    run<2>();
//    run<3>();
//    run<4>();
//    run<5>();
//    run<6>();

    return 0;
}
