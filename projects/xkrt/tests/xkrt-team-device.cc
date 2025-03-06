# include <xkrt/xkrt.h>
# include <assert.h>

static xkrt_runtime_t runtime;
static volatile int run_for_device[XKRT_DEVICES_MAX];

static void *
run(xkrt_team_t * team, xkrt_thread_t * thread)
{
    assert(thread->tid >= 0);
    assert(thread->tid < runtime.drivers.devices.n);
    run_for_device[thread->tid] = 1;
    return NULL;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    xkrt_team_t team = {
        .desc = {
            .routine = run,
            .args = NULL,
            .nthreads = runtime.drivers.devices.n,
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_DEVICE,
            }
        }
    };

    runtime.team_create(&team);
    runtime.team_join(&team);

    for (int i = 0 ; i < runtime.drivers.devices.n ; ++i)
        assert(run_for_device[i] == 1);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
