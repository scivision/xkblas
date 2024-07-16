#ifndef __REGION_HPP__
# define __REGION_HPP__

class Region {

    public:
        Region() {}
        virtual ~Region() {}

        # if 0
        virtual inline void     copy(const Region & other)                  = 0;
        virtual inline Region   intersection(const Region & region) const   = 0;

        virtual inline bool intersects(Region & region) const = 0;
        virtual inline bool equals    (Region & region) const = 0;
        virtual inline bool includes  (Region & region) const = 0;
        virtual inline bool is_empty  (void)            const = 0;
        # endif

}; /* class Region */

#endif /* __REGION_HPP__ */
