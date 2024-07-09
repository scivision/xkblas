# include "demangle.hpp"
# include "region.hpp"
# include "task.hpp"
# include "task-access-interval-multi-tree.hpp"

# include <cmath>
# include <cstdlib>

# define T Task

// number of insertion counter
static int ninsert = 0;

// Insert a region into the history
template<int K>
static void
insert(
    History<K> & history,
    access_mode_t mode,
    interval_t intervals[K]
) {
    Region<K> region(intervals);
    Task * task = task_new();
    history.intersect(mode, region, task);
    history.insert(mode, region, task);
    ++ninsert;
//    std::cout << "inserting " << (mode == OUT ? "out" : mode == IN ? "in" : "unk") << " " << region << std::endl;
}

//Generate 'n' disjoint hyperplans
template<int K, int PLAN_DIM>
void
disjoint_hyperplans(History<K> & history, int n)
{
    static_assert(0 <= PLAN_DIM && PLAN_DIM < K);

    for (int i = 0 ; i < n ; ++i)
    {
        interval_t intervals[K];
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
        insert(history, OUT, intervals);
    }
}

//Generate 'n' hypercubes that includes all dimensions but one
template<int K>
void
pyramid(History<K> & history, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        interval_t intervals[K];
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
        insert(history, OUT, intervals);
    }
}

//Generate 'n' hypercubes that are included on all dimensions but one
template<int K>
void
pyramid_inverted(History<K> & history, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        interval_t intervals[K];
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
        insert(history, OUT, intervals);
    }
}

// Generate 'n' hypercubes that are successively included into previous ones
template<int K>
static void
squares_included(History<K> & history, int n)
{
    for (int i = 0 ; i < n ; ++i)
    {
        interval_t intervals[K];
        for (int k = 0 ; k < K ; ++k)
        {
            intervals[k].a =     i*4;
            intervals[k].b = n*8-i*4;
        }
        insert<K>(history, OUT, intervals);
    }
}

template<int Dimensions, int K, class Callable>
constexpr void meta_for_loop(History<K> & history, std::array<int, K> & array, int end, Callable & c)
{
    static_assert(Dimensions > 0);
    for(int i = 0; i != end; ++i)
    {
        array[Dimensions-1] = i;
        if constexpr(Dimensions > 1)
            meta_for_loop<Dimensions-1, K>(history, array, end, c);
        else
            c(history, array);
    }
}

template<int K, int P>
static void
matrix_tiles_insert(History<K> & history, std::array<int, K> indices)
{
    interval_t intervals[K];
    for (int k = 0 ; k < K ; ++k)
    {
        intervals[k].a = (indices[k]+0)*P;
        intervals[k].b = (indices[k]+1)*P;
    }
    insert(history, OUT, intervals);
}

// Generate n^K tiles of size P
template<int K, int P>
static void
matrix_tiles(History<K> & history, int n)
{
    std::array<int, K> array;
    meta_for_loop<K, K>(history, array, n, matrix_tiles_insert<K, P>);
}

// Test size
static int N = 10;

// Launch tests for a history of dimension 'K'
template<int K>
static void launch_tests(History<K> & history)
{
    printf("Running for K=%d and structure %s\n", K, demangle(history).c_str());
    ninsert = 0;
    task_clear();

    uint64_t t0 = get_nanotime();
    {

        # if 0
        for (int i = 0 ; i < N ; ++i)
        {
            interval_t interval[K] = {
                { .a = 0,  .b = 16      },
                { .a = 4*i, .b = 4*(i+1) },
            };
            insert<K>(history, OUT, interval);
        }

        int xx[] = {
            4, 1024,
            1028,2048,
            2564, 4096,
            2052,2304,
            2308,2432,
            2436,2496,
            2500, 2528,
            2532,2544,
        };

        for (unsigned int i = 0 ; i < sizeof(xx) / sizeof(int) ; i += 2)
        {
            interval_t interval[K] = {
                { .a = 0,  .b = 16  },
                { .a = xx[i+0],  .b = xx[i+1] },
            };
            insert<K>(history, OUT, interval);
        }

        # else

        // hyperplans
        disjoint_hyperplans<K, K-1>(history, N);
        disjoint_hyperplans<K,   0>(history, N);

        // matrix test
        int Nth_sqrt = std::pow(N, 1.0/K);
        matrix_tiles<K, 2>(history, Nth_sqrt/2*2+1);
        matrix_tiles<K, 3>(history, Nth_sqrt/3*2+1);
        matrix_tiles<K, 5>(history, Nth_sqrt/5*2+1);
        matrix_tiles<K, 7>(history, Nth_sqrt/7*2+1);

        // include squares
        squares_included<K>(history, N);

        // pyramid inverted
        pyramid_inverted<K>(history, N);

        // pyramid
        pyramid<K>(history, N);

        # endif
    }
    uint64_t tf = get_nanotime();

    // finish
    double dt = (tf - t0) / 1e9;
    int nelements = history.size();
    int nedges = task_nedges();
    printf("Took %lf s.\n", dt);
    printf("    Inserted %d regions and %d elements\n", ninsert, nelements);
    printf("        Regions/s. = %.2lf\n", ninsert / dt);
    printf("        Elements/s. = %.2lf\n", nelements / dt);
    printf("    Set %d task edges\n", nedges);
    printf("        Edges/s. = %.2lf\n", nedges / dt);
    printf("\n");
}

// Run for dimension 'K'
template<int K>
static void run(void)
{
    TaskAccessIntervalMultiTree<K> tree;
    launch_tests(tree);

    # ifdef EXPORT_PDF
    std::cout << "Exporting pdf..." << std::endl;
    tree.export_pdf();
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
