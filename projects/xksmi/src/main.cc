# include <xkrt/xkrt.h>
# include <xkrt/logger/metric.h>

# include <xksmi/xksmi-version.h>

# include <ncurses.h>

static void
print_usage(const char * prog_name)
{
    LOGGER_INFO("Usage: %s [-f fps] [-h]", prog_name);
    LOGGER_INFO("Options:");
    LOGGER_INFO("  -f fps        Frequency to update");
    LOGGER_INFO("  -h            Show this help message");
}

// separators for rendering
static const char * sep0 = "                                                                                                                                                                                                                                                                                                ";
static const char * sep1 = "##########################################################################################################################################################################################################################################################################################################################";
static const char * sep2 = "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";
static const char * title = "XKSMI " XKSMI_VERSION_GITHASH;
static const int titlelen = strlen(title);

// main loop, both updating and rendering
static void
loop(xkrt_runtime_t * runtime, int target_fps)
{
    // frame counter
    uint64_t nframes = 0;

    // timestamp since last real 'fps' update
    uint64_t tfps = xkrt_get_nanotime();

    // ns per frame to reach the target fps
    const uint64_t target_nspf = 1000000000 / target_fps;

    // fps on which we are running, updated once per second
    double fps = target_fps;

    while (1)
    {
        // clear previous frame
        clear();

        // ncurses screen position
        const int x = 0;
              int y = 0;

        // get screen width and height
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        # define printsep1() mvprintw(y++, x, "%.*s", cols, sep1)
        # define printsep2() mvprintw(y++, x, "%.*s", cols, sep2)

        // print title
        const int nspaces = (cols - titlelen);
        mvprintw(y++, x, "%*.s%s", nspaces, sep0, title);

        // time to draw this frame
        const uint64_t t0 = xkrt_get_nanotime();

        // for each driver
        for (int driver_type = 0 ; driver_type != XKRT_DRIVER_TYPE_MAX ; ++driver_type)
        {
            xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t)driver_type);
            if (driver == NULL)
                continue ;

            printsep1();

            // print driver name
            if (driver->f_get_name)
                mvprintw(y++, x, "Driver `%s`", driver->f_get_name());
            else
                mvprintw(y++, x, "Driver `%d`", driver_type);

            // print all devices
            for (int i = 0 ; i < driver->ndevices_commited ; ++i)
            {
                xkrt_device_t * device = driver->devices[i];
                if (device == NULL)
                    continue ;

                printsep2();
                mvprintw(y++, x, "Device %u of global id %u", device->driver_id, device->global_id);

                if (driver->f_memory_device_info)
                {
                    xkrt_device_memory_info_t meminfo[XKRT_DEVICE_MEMORIES_MAX];
                    int nmemories = 0;
                    driver->f_memory_device_info(device->driver_id, meminfo, &nmemories);

                    for (int j = 0 ; j < nmemories ; ++j)
                    {
                        xkrt_device_memory_info_t * mem = meminfo + j;

                        char memory_used_str[64];
                        xkrt_metric_byte(memory_used_str, sizeof(memory_used_str), mem->used);

                        char memory_total_str[64];
                        xkrt_metric_byte(memory_total_str, sizeof(memory_used_str), mem->capacity);

                        mvprintw(
                            y++, x,
                            "Memory %d - usage %s/%s",
                            j, memory_used_str, memory_total_str
                        );
                    }
                }
            }
        }

        // update frame counter
        ++nframes;

        // update fps every seconds
        if (nframes % target_fps == 0)
        {
            const double elapsed = (xkrt_get_nanotime() - tfps) / (double) 1e9;
            fps = target_fps / (double) elapsed;
            // LOGGER_DEBUG("Running at %.0lf FPS", fps);
            tfps = xkrt_get_nanotime();
        }
        mvprintw((y += 2), x, "Running at %.0lf FPS - increase rate with [-f fps]", fps);

        // print on screen
        refresh();

        // sleep until next frame
        const uint64_t tf = xkrt_get_nanotime();
        const uint64_t elapsed = tf - t0;
        if (elapsed < target_nspf)
        {
            const uint64_t sleepfor = target_nspf - elapsed;
            // LOGGER_DEBUG("Sleeping for %.2lf s", sleepfor / 1000000000.0);
            usleep((useconds_t) sleepfor / 1000);
        }
    } /* while (1) */

}

int
main(int argc, char ** argv)
{
    // parse CLI
    int fps = 1;

    int opt;
    while ((opt = getopt(argc, argv, "f:h")) != -1)
    {
        switch (opt)
        {
            case 'f':
            {
                fps = atoi(optarg);
                break;
            }

            case 'h':
            {
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            }

            default:
            {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }

    // init ncurses
    initscr();            // Start ncurses mode
    noecho();             // Don't echo keypresses
    curs_set(FALSE);      // Hide the cursor

    // init runtime
    xkrt_runtime_t runtime;
    xkrt_init(&runtime);

    // main loop
    loop(&runtime, fps);

    // finish ncurses
    endwin();             // End ncurses mode (unreachable here)

    // deinit runtime
    xkrt_deinit(&runtime);

    return 0;
}
