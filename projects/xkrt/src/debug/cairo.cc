/* ************************************************************************** */
/*                                                                            */
/*   cairo.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/04/03 16:06:32 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:54:43 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/support.h>

# if XKRT_SUPPORT_CAIRO
#  include <cairo/cairo.h>
#  include <cairo/cairo-svg.h>
#  include <xkrt/memory/access/blas/memory-tree.hpp>
void
xkrt_cairo_memory_trees(
    xkrt_runtime_t * runtime
) {
    for (MemoryCoherencyController * memcontroller : runtime->memcontrollers)
    {
        BLASBLASMemoryTree * memtree = (BLASBLASMemoryTree *) memcontroller;
        assert(memtree);

        if (memtree->root == NULL)
            continue ;

        // Step 1: Use a recording surface (size is dynamically determined)
        cairo_surface_t *rec_surface = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, NULL);
        cairo_t *rec_ctx = cairo_create(rec_surface);

        // Step 2: Set stroke properties (line width and color)
        cairo_set_line_width(rec_ctx, 3); // Border thickness
        cairo_set_source_rgb(rec_ctx, 0, 0, 0); // Black stroke

        // Step 2: Draw elements without knowing the final size
        double offset = (double) memtree->root->rect[1].a;
        std::function<void(BLASBLASMemoryTree::NodeBase *, void *)> f = [offset, rec_ctx](BLASBLASMemoryTree::NodeBase * node, void * args) {
            double y1 = (double) node->rect[0].a;
            double y2 = (double) node->rect[0].b;
            double x2 = (double) offset - (double) node->rect[1].a;
            double x1 = (double) offset - (double) node->rect[1].b;
            cairo_rectangle(rec_ctx, x1, y1, x2-x1, y2-y1);
            cairo_stroke(rec_ctx);
            LOGGER_WARN("Drawing %lf %lf %lf %lf", x1, y1, x2-x1, y2-y1);
            (void) args;
        };
        memtree->foreach_node(f, NULL);

        // Step 3: Get the bounding box of everything drawn
        double x1, y1, x2, y2;
        cairo_recording_surface_ink_extents(rec_surface, &x1, &y1, &x2, &y2);

        // Step 4: Create a properly sized SVG surface
        char filename[128];
        snprintf(filename, sizeof(filename), "memtree-%lu-%lu.svg", memtree->ld, memtree->sizeof_type);
        cairo_surface_t *svg_surface = cairo_svg_surface_create(filename, x2, y2);
        cairo_t *svg_ctx = cairo_create(svg_surface);
        LOGGER_WARN("Exporting memory tree to `%s`", filename);

        // Step 5: Replay recorded drawing onto the final surface
        cairo_set_source_surface(svg_ctx, rec_surface, 0, 0);
        cairo_paint(svg_ctx);

        // Cleanup
        cairo_destroy(rec_ctx);
        cairo_surface_destroy(rec_surface);
        cairo_destroy(svg_ctx);
        cairo_surface_destroy(svg_surface);
    }
}

# endif /* XKRT_SUPPORT_CAIRO */

