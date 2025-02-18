# ifndef __COMBINATOR_H__
#  define __COMBINATOR_H__

# include <initializer_list>
# include <algorithm>
# include <vector>
# include <iostream>

// Generates the 2**n bitmask combinations of the given bitsets
template<typename T>
struct combinator_t : std::vector<T>
{
    const std::vector<T> values;
    const std::vector<const char *> names;

    combinator_t(
        const std::initializer_list<T> values,
        const std::initializer_list<const char *> names = {}
    ) : values(values), names(names) {
        const size_t n = values.size();
        const size_t ncombinations = (1 << n) - 1;
        this->reserve(ncombinations);

        for (int r = 1 ; r <= n ; ++r)
        {
            std::vector<bool> v(n);
            std::fill(v.end() - r, v.end(), true);

            do {
                T mask = 0;
                for (int i = 0; i < n; ++i)
                {
                    if (v[i])
                        mask |= values.begin()[i];
                }
                this->push_back(mask);
            } while (std::next_permutation(v.begin(), v.end()));

        }
        assert(this->size() == ncombinations);
    }

    void names_from_flags(char * buffer, const size_t size, const T & flags) const
    {
        assert(buffer && size > 0);

        size_t len = 0;
        for (int i = 0 ; i < this->values.size() ; ++i)
        {
            const T & flag = this->values[i];
            if (flags & flag)
            {
                const char * name = this->names[i];
                len += snprintf(buffer + len, size - len - 1, len == 0 ? "%s" : " | %s", name);
                if (len == size)
                    break ;
            }
        }
    }
};

# endif /* __COMBINATOR_H__ */
