# include <xkrt/memory/khp-tree.hpp>

# define unused_type_t  int
# define CUT            true

template<int K>
class NoopKHPTree : public KHPTree<K, unused_type_t, CUT>
{
    using Cube = KCube<K>;
    using Node = typename KHPTree<K, unused_type_t, CUT>::Node;

    Node *
    new_node(
    unused_type_t & t,
        const Cube & cube,
        const int k,
        const Color color
    ) const {
        return new Node(cube, k, color);
    }

    Node *
    new_node(
        unused_type_t & t,
        const Cube & cube,
        const int k,
        const Color color,
        const Node * inherit
    ) const {
        return new Node(cube, k, color);
    }

    bool
    should_cut(unused_type_t & t, Cube & cube, Node * parent, int k) const
    {
        return false;
    }

    void
    on_insert(Node * node, unused_type_t & t)
    {
    }

    void
    on_shrink(Node * node, const Interval & interval, int k)
    {
    }

    bool
    intersect_stop_test(Node * node, unused_type_t & t, const Cube & cube) const
    {
        return false;
    }

    void
    on_intersect(Node * node, unused_type_t & t, const Cube & cube) const
    {
    }
};
