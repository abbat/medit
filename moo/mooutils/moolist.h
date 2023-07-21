#ifndef MOO_LIST_H
#define MOO_LIST_H

#include <mooglib/moo-glib.h>

#define _MOO_DEFINE_LIST(GListType, glisttype,                          \
                         ListType, list_type, ElmType)                  \
                                                                        \
typedef struct ListType ListType;                                       \
struct ListType {                                                       \
    ElmType *data;                                                      \
    ListType *next;                                                     \
};                                                                      \
                                                                        \
static inline ListType *                                                \
list_type##_from_g##glisttype (GListType *list)                         \
{                                                                       \
    return (ListType*) list;                                            \
}                                                                       \
                                                                        \
static inline GListType *                                               \
list_type##_to_g##glisttype (ListType *list)                            \
{                                                                       \
    return (GListType*) list;                                           \
}                                                                       \
                                                                        \
static inline void                                                      \
list_type##_free_links (ListType *list)                                 \
{                                                                       \
    g_##glisttype##_free ((GListType*) list);                           \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_copy_links (ListType *list)                                 \
{                                                                       \
    return (ListType*) g_##glisttype##_copy ((GListType*) list);        \
}                                                                       \
                                                                        \
static inline guint                                                     \
list_type##_length (ListType *list)                                     \
{                                                                       \
    return g_##glisttype##_length ((GListType*) list);                  \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_reverse (ListType *list)                                    \
{                                                                       \
    return (ListType*) g_##glisttype##_reverse ((GListType*) list);     \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_prepend (ListType *list, ElmType *data)                     \
{                                                                       \
    return (ListType*) g_##glisttype##_prepend ((GListType*)list, data);\
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_append (ListType *list, ElmType *data)                      \
{                                                                       \
    return (ListType*) g_##glisttype##_append ((GListType*)list, data); \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_concat (ListType *list1, ListType *list2)                   \
{                                                                       \
    return (ListType*) g_##glisttype##_concat ((GListType*) list1,      \
                                       (GListType*) list2);             \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_remove (ListType *list, const ElmType *data)                \
{                                                                       \
    return (ListType*) g_##glisttype##_remove ((GListType*)list, data); \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_delete_link (ListType *list, ListType *link)                \
{                                                                       \
    return (ListType*)                                                  \
        g_##glisttype##_delete_link ((GListType*)list,                  \
                                     (GListType*)link);                 \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_find (ListType *list, const ElmType *data)                  \
{                                                                       \
    while (list)                                                        \
    {                                                                   \
        if (list->data == data)                                         \
            return list;                                                \
        list = list->next;                                              \
    }                                                                   \
    return NULL;                                                        \
}                                                                       \
                                                                        \
static inline ListType *                                                \
list_type##_find_custom (ListType      *list,                           \
                         gconstpointer  data,                           \
                         GCompareFunc   func)                           \
{                                                                       \
    return (ListType*) g_##glisttype##_find_custom ((GListType*) list,  \
                                            data, func);                \
}                                                                       \
                                                                        \
typedef void (*ListType##Func) (ElmType *data, gpointer user_data);     \
                                                                        \
static inline void                                                      \
list_type##_foreach (ListType      *list,                               \
                     ListType##Func func,                               \
                     gpointer       user_data)                          \
{                                                                       \
    g_##glisttype##_foreach ((GListType*) list,                         \
                             (GFunc) func, user_data);                  \
}

#define MOO_DEFINE_LIST_COPY_FUNC(ListType, list_type, elm_copy_func)   \
static inline ListType *                                                \
list_type##_copy (ListType *list)                                       \
{                                                                       \
    ListType *copy = NULL;                                              \
    while (list)                                                        \
    {                                                                   \
        copy = list_type##_prepend (copy, elm_copy_func (list->data));  \
        list = list->next;                                              \
    }                                                                   \
    return list_type##_reverse (copy);                                  \
}

#define MOO_DEFINE_LIST_FREE_FUNC(ListType, list_type, elm_free_func)   \
static inline void                                                      \
list_type##_free (ListType *list)                                       \
{                                                                       \
    ListType *l;                                                        \
    for (l = list; l != NULL; l = l->next)                              \
        elm_free_func (l->data);                                        \
    list_type##_free_links (list);                                      \
}

#define MOO_DEFINE_SLIST(ListType, list_type, ElmType)                  \
    _MOO_DEFINE_LIST(GSList, slist, ListType, list_type, ElmType)

#define MOO_DEFINE_DLIST(ListType, list_type, ElmType)                  \
    _MOO_DEFINE_LIST(GList, list, ListType, list_type, ElmType)

#define MOO_DEFINE_SLIST_FULL(ListType, list_type, ElmType,             \
                              elm_copy_func, elm_free_func)             \
    MOO_DEFINE_SLIST(ListType, list_type, ElmType)                      \
    MOO_DEFINE_LIST_COPY_FUNC(ListType, list_type, elm_copy_func)       \
    MOO_DEFINE_LIST_FREE_FUNC(ListType, list_type, elm_free_func)

#define MOO_DEFINE_DLIST_FULL(ListType, list_type, ElmType,             \
                              elm_copy_func, elm_free_func)             \
    MOO_DEFINE_DLIST(ListType, list_type, ElmType)                      \
    MOO_DEFINE_LIST_COPY_FUNC(ListType, list_type, elm_copy_func)       \
    MOO_DEFINE_LIST_FREE_FUNC(ListType, list_type, elm_free_func)

#ifdef __cplusplus
#define _MOO_DEFINE_QUEUE_FOREACH(Element, element)                     \
template<typename T> static inline void                                 \
element##_queue_foreach (Element##Queue *queue,                         \
                         void (*func) (Element *elm, T *data),          \
                         T *data)                                       \
{                                                                       \
    g_queue_foreach (element##_queue_to_gqueue (queue),                 \
                     (GFunc) func, data);                               \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_foreach (Element##Queue *queue,                         \
                         void (*func) (Element *elm, gpointer data),    \
                         void *data)                                    \
{                                                                       \
    g_queue_foreach (element##_queue_to_gqueue (queue),                 \
                     (GFunc) func, data);                               \
}
#else
#define _MOO_DEFINE_QUEUE_FOREACH(Element, element)                     \
static inline void                                                      \
element##_queue_foreach (Element##Queue *queue,                         \
                         void (*func) (Element *elm, void *data),       \
                         void *data)                                    \
{                                                                       \
    g_queue_foreach (element##_queue_to_gqueue (queue),                 \
                     (GFunc) func, data);                               \
}
#endif

#define MOO_DEFINE_QUEUE(Element, element)                              \
                                                                        \
MOO_DEFINE_DLIST(Element##List, element##_list, Element)                \
                                                                        \
typedef struct Element##Queue Element##Queue;                           \
struct Element##Queue {                                                 \
  Element##List *head;                                                  \
  Element##List *tail;                                                  \
  guint length;                                                         \
};                                                                      \
                                                                        \
static inline Element##Queue *                                          \
element##_queue_from_gqueue (GQueue *queue)                             \
{                                                                       \
    return (Element##Queue*) queue;                                     \
}                                                                       \
                                                                        \
static inline GQueue *                                                  \
element##_queue_to_gqueue (Element##Queue *queue)                       \
{                                                                       \
    return (GQueue*) queue;                                             \
}                                                                       \
                                                                        \
static inline Element##Queue *                                          \
element##_queue_new (void)                                              \
{                                                                       \
    return element##_queue_from_gqueue (g_queue_new ());                \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_free_links (Element##Queue *queue)                      \
{                                                                       \
    g_queue_free (element##_queue_to_gqueue (queue));                   \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_push_head (Element##Queue *queue, Element *elm)         \
{                                                                       \
    g_queue_push_head (element##_queue_to_gqueue (queue), elm);         \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_push_tail (Element##Queue *queue, Element *elm)         \
{                                                                       \
    g_queue_push_tail (element##_queue_to_gqueue (queue), elm);         \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_push_head_link (Element##Queue *queue,                  \
                                Element##List  *link_)                  \
{                                                                       \
    g_queue_push_head_link (element##_queue_to_gqueue (queue),          \
                            element##_list_to_glist (link_));           \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_push_tail_link (Element##Queue *queue,                  \
                                Element##List  *link_)                  \
{                                                                       \
    g_queue_push_tail_link (element##_queue_to_gqueue (queue),          \
                            element##_list_to_glist (link_));           \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_unlink (Element##Queue *queue,                          \
                        Element##List  *link_)                          \
{                                                                       \
    g_queue_unlink (element##_queue_to_gqueue (queue),                  \
                    element##_list_to_glist (link_));                   \
}                                                                       \
                                                                        \
static inline void                                                      \
element##_queue_delete_link (Element##Queue *queue,                     \
                             Element##List  *link_)                     \
{                                                                       \
    g_queue_delete_link (element##_queue_to_gqueue (queue),             \
                         element##_list_to_glist (link_));              \
}                                                                       \
                                                                        \
_MOO_DEFINE_QUEUE_FOREACH(Element, element)

#endif /* MOO_LIST_H */
