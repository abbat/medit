/*
 *   mooutils-cpp.h
 *
 *   Copyright (C) 2004-2015 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#ifdef __cplusplus

namespace g {

template<typename T>
inline T *object_ref(T *obj)
{
    return static_cast<T*>(g_object_ref(obj));
}

} // namespace g

/*
 * The operators a flags enum needs, with one asymmetry that matters: ~ returns
 * an int and not the enum. An unscoped enum can only hold the values its
 * enumerators span -- MooEditStatus runs to 16, so five bits -- and ~ sets every
 * other bit of the word, which no value of the type can represent. Making that
 * an enum value is undefined behaviour, reported by clang's -fsanitize=enum as
 * "load of value 4294967291, which is not a valid value for type
 * 'MooEditStatus'"; gcc has no such check and said nothing for years.
 *
 * As an int it is a mask, which is what it was always meant to be, and the &
 * that follows brings the result back into the enum: f & ~g can only have bits
 * f already had.
 */
#define MOO_DEFINE_FLAGS(Flags)                                                                                 \
    inline Flags operator | (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) | f2); }      \
    inline Flags operator & (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) & f2); }      \
    inline Flags operator & (Flags f1, int mask) { return static_cast<Flags>(static_cast<int>(f1) & mask); }    \
    inline Flags& operator |= (Flags& f1, Flags f2) { f1 = f1 | f2; return f1; }                                \
    inline Flags& operator &= (Flags& f1, Flags f2) { f1 = f1 & f2; return f1; }                                \
    inline Flags& operator &= (Flags& f1, int mask) { f1 = f1 & mask; return f1; }                              \
    inline int operator ~ (Flags f) { return ~static_cast<int>(f); }                                            \


template<typename GObjType>
struct ObjectTraits;

namespace g {

template<typename GObjType>
inline GObjType* object_new()
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), nullptr);
}

template<typename GObjType, typename Arg1>
inline GObjType* object_new(const char* prop1, Arg1 arg1)
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), prop1, arg1, nullptr);
}

template<typename GObjType, typename Arg1, typename Arg2>
inline GObjType* object_new(const char* prop1, Arg1 arg1, const char* prop2, Arg2 arg2)
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), prop1, arg1, prop2, arg2, nullptr);
}

} // namespace g

#define MOO_DEFINE_GOBJ_TRAITS(FooObject, FOO_TYPE_OBJECT)      \
    template<>                                                  \
    struct ObjectTraits<FooObject>                              \
    {                                                           \
        using CType = FooObject;                                \
        static GType gtype()                                    \
        {                                                       \
            return FOO_TYPE_OBJECT;                             \
        }                                                       \
    };

MOO_DEFINE_GOBJ_TRAITS(GObject, G_TYPE_OBJECT);


template<typename Owner, typename Value>
class ObjectDataAccessor
{
public:
    ObjectDataAccessor(const char* prop_name)
        : m_prop_name(prop_name)
        , m_prop_quark(0)
    {
    }

    ObjectDataAccessor(GQuark prop_quark)
        : m_prop_name(nullptr)
        , m_prop_quark(prop_quark)
    {
    }

    Value get(Owner* owner) const
    {
        return (Value) (m_prop_name ?
                        g_object_get_data (G_OBJECT (owner), m_prop_name) :
                        g_object_get_qdata (G_OBJECT (owner), m_prop_quark));
    }

    void set(Owner* owner, Value value) const
    {
        if (m_prop_name)
            g_object_set_data (G_OBJECT (owner), m_prop_name, value);
        else
            g_object_set_qdata (G_OBJECT (owner), m_prop_quark, value);
    }

    void set(Owner* owner, Value value, GDestroyNotify notify) const
    {
        if (m_prop_name)
            g_object_set_data_full (G_OBJECT (owner), m_prop_name, value, notify);
        else
            g_object_set_qdata_full (G_OBJECT (owner), m_prop_quark, value, notify);
    }

    ObjectDataAccessor(const ObjectDataAccessor&) = delete;
    ObjectDataAccessor& operator=(const ObjectDataAccessor&) = delete;

private:
    const char* m_prop_name;
    const GQuark m_prop_quark;
};

template<typename Owner>
class ObjectDataAccessor<Owner, bool>
{
public:
    ObjectDataAccessor(const char* prop_name)
        : m_prop_name(prop_name)
    {
    }

    bool get(Owner* owner) const
    {
        return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (owner), m_prop_name));
    }

    void set(Owner* owner, bool value) const
    {
        g_object_set_data (G_OBJECT (owner), m_prop_name, GINT_TO_POINTER (value));
    }

    void set(Owner* owner, bool value, GDestroyNotify notify) const
    {
        g_object_set_data_full (G_OBJECT (owner), m_prop_name, GINT_TO_POINTER (value), notify);
    }

    ObjectDataAccessor(const ObjectDataAccessor&) = delete;
    ObjectDataAccessor& operator=(const ObjectDataAccessor&) = delete;

private:
    const char* m_prop_name;
};


MOO_DEFINE_FLAGS(GRegexCompileFlags)
MOO_DEFINE_FLAGS(GRegexMatchFlags)
MOO_DEFINE_FLAGS(GParamFlags)
MOO_DEFINE_FLAGS(GSignalFlags)
MOO_DEFINE_FLAGS(GdkDragAction)
MOO_DEFINE_FLAGS(GdkModifierType)


#endif // __cplusplus
