#pragma once

#ifdef __cplusplus

template<typename ElmType>
struct MooArrayNoCopy
{
    ElmType *operator() (ElmType *elm) const { return elm; }
};

template<typename ElmType>
struct MooArrayNoFree
{
    void operator() (ElmType *elm) const { (void) elm; }
};

template<typename ElmType>
struct MooObjectArrayCopy
{
    ElmType *operator() (ElmType *elm) const
    {
        return static_cast<ElmType *> (g_object_ref (elm));
    }
};

template<typename ElmType>
struct MooObjectArrayFree
{
    void operator() (ElmType *elm) const { g_object_unref (elm); }
};

template<typename ElmType,
         typename Copy = MooArrayNoCopy<ElmType>,
         typename Free = MooArrayNoFree<ElmType>>
class MooArray
{
public:
    ElmType **elms;
    gsize n_elms;

    MooArray () : elms (nullptr), n_elms (0), n_allocd (0) {}

    ~MooArray ()
    {
        clear ();
        g_free (elms);
    }

    MooArray (const MooArray &other) : MooArray () { append_array (&other); }

    MooArray (MooArray &&other) noexcept
        : elms (other.elms), n_elms (other.n_elms), n_allocd (other.n_allocd)
    {
        other.elms = nullptr;
        other.n_elms = 0;
        other.n_allocd = 0;
    }

    MooArray &operator= (const MooArray &other)
    {
        if (this != &other)
        {
            clear ();
            append_array (&other);
        }
        return *this;
    }

    MooArray &operator= (MooArray &&other) noexcept
    {
        if (this != &other)
        {
            clear ();
            g_free (elms);
            elms = other.elms;
            n_elms = other.n_elms;
            n_allocd = other.n_allocd;
            other.elms = nullptr;
            other.n_elms = 0;
            other.n_allocd = 0;
        }
        return *this;
    }

    static MooArray *create () { return new MooArray (); }
    static void destroy (MooArray *array) { delete array; }
    MooArray *copy () const { return new MooArray (*this); }

    void append (ElmType *elm)
    {
        g_return_if_fail (elm != nullptr);
        grow (1);
        elms[n_elms - 1] = Copy () (elm);
    }

    void take (ElmType *elm)
    {
        g_return_if_fail (elm != nullptr);
        grow (1);
        elms[n_elms - 1] = elm;
    }

    void append_array (const MooArray *other)
    {
        gsize old_size;

        g_return_if_fail (other != nullptr);
        if (other->n_elms == 0)
            return;
        old_size = n_elms;
        grow (other->n_elms);
        for (gsize i = 0; i < other->n_elms; ++i)
            elms[old_size + i] = Copy () (other->elms[i]);
    }

    void remove (ElmType *elm)
    {
        g_return_if_fail (elm != nullptr);
        for (gsize i = 0; i < n_elms; ++i)
        {
            if (elms[i] == elm)
            {
                if (i + 1 < n_elms)
                    std::move (elms + i + 1, elms + n_elms, elms + i);
                --n_elms;
                Free () (elm);
                return;
            }
        }
    }

    void clear ()
    {
        /*
         * The count goes to zero before anything is freed. Free() is an unref
         * on every instantiation in the tree, an unref runs the object's
         * dispose, and dispose emits signals whose handlers reach back into
         * the object that owns this array -- which would find a live count
         * over pointers that have already been freed.
         */
        ElmType **freeing = elms;
        gsize n_freeing = n_elms;

        n_elms = 0;

        for (gsize i = 0; i < n_freeing; ++i)
            Free () (freeing[i]);
    }

    void sort (GCompareFunc func)
    {
        g_return_if_fail (func != nullptr);
        if (n_elms > 1)
            g_qsort_with_data (elms, n_elms, sizeof (*elms), compare, (gpointer) func);
    }

    gssize find (ElmType *elm) const
    {
        g_return_val_if_fail (elm != nullptr, -1);
        for (gsize i = 0; i < n_elms; ++i)
            if (elms[i] == elm)
                return (gssize) i;
        return -1;
    }

    gsize insert_sorted (ElmType *elm, GCompareFunc func)
    {
        gsize i = 0;

        g_return_val_if_fail (elm != nullptr && func != nullptr, 0);
        while (i < n_elms && func (elms[i], elm) <= 0)
            ++i;
        grow (1);
        if (i < n_elms - 1)
            std::move_backward (elms + i, elms + n_elms - 1, elms + n_elms);
        elms[i] = Copy () (elm);
        return i;
    }

    template<typename Func>
    void foreach (Func func, gpointer data) const
    {
        g_return_if_fail (func != nullptr);
        for (gsize i = 0; i < n_elms; ++i)
            func (elms[i], data);
    }

    bool empty () const { return n_elms == 0; }
    gsize size () const { return n_elms; }
    ElmType *operator[] (gsize index) const { return elms[index]; }

private:
    gsize n_allocd;

    void grow (gsize amount)
    {
        gsize required = n_elms + amount;
        if (required > n_allocd)
        {
            n_allocd = std::max (gsize (n_allocd * 1.2), required);
            elms = static_cast<ElmType **> (g_realloc (elms, n_allocd * sizeof (*elms)));
        }
        std::fill (elms + n_elms, elms + required, nullptr);
        n_elms = required;
    }

    static gint compare (gconstpointer a, gconstpointer b, gpointer data)
    {
        GCompareFunc func = (GCompareFunc) data;
        return func (*(ElmType **) a, *(ElmType **) b);
    }
};

template<typename ElmType>
using MooObjectArray = MooArray<ElmType, MooObjectArrayCopy<ElmType>, MooObjectArrayFree<ElmType>>;

#endif /* __cplusplus */
