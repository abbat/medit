/*
 *   moolua/mooluaplugin.cpp
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *                 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#include "mooluaplugin.h"

#include "medit-lua.h"
#include "mooedit/mooplugin-loader.h"
#include "mooedit/mooplugin-macro.h"
#include "moolua/lua-module-init.h"
#include "moolua/lua-plugin-init.h"
#include "mooutils/mooi18n.h"

/*!< \brief Identifier for the MooLua plugin */
const char *LUA_PLUGIN_ID = "MooLua";

/*!
 * \brief Structure representing a Lua module
 */
struct LuaModule
{
  lua_State *L; /*!< \brief Lua state instance */
};

/*!< \brief Define a singly-linked list for Lua modules */
MOO_DEFINE_SLIST (ModuleList, module_list, LuaModule)

/*!
 * \brief Structure representing a Lua plugin
 */
struct LuaPlugin
{
  MooPlugin parent;    /*!< \brief Parent plugin structure */
  ModuleList *modules; /*!< \brief List of loaded modules */
};

/*!< \brief Define plugin information for the Lua plugin */
MOO_PLUGIN_DEFINE_INFO (lua, N_ ("Lua"), N_ ("Lua support"), "Yevgen Muntyan <" MOO_EMAIL ">", MOO_VERSION)

/*!< \brief Define the Lua plugin type and implementation */
MOO_PLUGIN_DEFINE (Lua, lua, NULL, NULL, NULL, NULL, NULL, 0, 0)

/*!
 * \brief Load a Lua module from file
 *
 * \param filename Path to the Lua module file
 * \return Pointer to the loaded Lua module, or NULL on failure
 */
static LuaModule *
lua_module_load (const char *filename)
{
  lua_State *L = medit_lua_new ();

  if (!L)
    return NULL;

  if (!medit_lua_do_string (L, LUA_MODULE_INIT))
    {
      medit_lua_free (L);
      return NULL;
    }

  if (!medit_lua_do_file (L, filename))
    {
      medit_lua_free (L);
      return NULL;
    }

  LuaModule *mod = g_new0 (LuaModule, 1);
  mod->L = L;

  return mod;
}

/*!
 * \brief Unload a Lua module and free its resources
 *
 * \param mod Pointer to the Lua module to unload
 */
static void
lua_module_unload (LuaModule *mod)
{
  g_return_if_fail (mod != NULL);
  if (mod->L)
    medit_lua_free (mod->L);

  g_free (mod);
}

/*!
 * \brief Initialize a Lua plugin
 *
 * \param plugin Pointer to the Lua plugin to initialize
 * \return TRUE on success, FALSE on failure
 */
static gboolean
lua_plugin_init (LuaPlugin *plugin)
{
  plugin->modules = NULL;
  return TRUE;
}

/*!
 * \brief Deinitialize a Lua plugin and unload all its modules
 *
 * \param plugin Pointer to the Lua plugin to deinitialize
 */
static void
lua_plugin_deinit (LuaPlugin *plugin)
{
  while (plugin->modules)
    {
      LuaModule *mod = plugin->modules->data;
      plugin->modules = module_list_delete_link (plugin->modules, plugin->modules);
      lua_module_unload (mod);
    }
}

/*!
 * \brief Load a module into a Lua plugin
 *
 * \param plugin Pointer to the Lua plugin
 * \param module_file Path to the module file to load
 */
static void
lua_plugin_load_module (LuaPlugin *plugin, const char *module_file)
{
  if (LuaModule *mod = lua_module_load (module_file))
    plugin->modules = module_list_prepend (plugin->modules, mod);
}

/*!
 * \brief Callback function to load a Lua module
 *
 * \param module_file Path to the module file to load
 * \param ini_file Configuration file (unused)
 * \param data User data (unused)
 */
static void
load_lua_module (const char *module_file, const char *ini_file, gpointer data)
{
  (void) data;
  (void) ini_file;

  if (LuaPlugin *plugin = (LuaPlugin *) moo_plugin_lookup (LUA_PLUGIN_ID))
    lua_plugin_load_module (plugin, module_file);
}

/*!
 * \brief Initialize the MooLua plugin system
 *
 * \return TRUE on success, FALSE on failure
 */
gboolean
moo_lua_plugin_init (void)
{
  MooPluginLoader loader = { load_lua_module, NULL, NULL };
  moo_plugin_loader_register (&loader, "Lua");
  MooPluginParams params = { TRUE, TRUE };

  return moo_plugin_register (LUA_PLUGIN_ID,
                              lua_plugin_get_type (),
                              &lua_plugin_info,
                              &params);
}
