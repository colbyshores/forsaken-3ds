/* vim:set sw=4 ts=4:
 *
 * Copyright (C) 2009  Pim Goossens
 *
 * Modified by Daniel Aquino (aka methods) for ProjectX
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

/* lua_config.h -- typed access to the Lua `config` table
 *
 * The getters read config[opt] from L1 and return the default when the key is
 * nil; the setters assign it. Values live only in the Lua state (loaded by
 * init.lua during lua_init) until config_save calls the Lua config_save in
 * scripts/config.lua, which writes the whole table to configs/main.txt
 * (configs/debug.txt with DEBUG_ON).
 *
 * config_get_int also accepts a boolean (0 / 1). config_get_bool follows Lua
 * truth: any value but false is true, including 0. A wrong-typed value for
 * the int, float or string getters raises a Lua error through luaL_check*
 * outside any protected call, which ends in Lua's panic handler.
 * config_get_strncpy uses strncpy, so dest is not terminated when the value
 * fills all size bytes.
 */

int config_save( void );

int config_get_int(const char *opt, int _default);
void config_set_int(const char *opt, int i);

float config_get_float(const char *opt, float _default);
void config_set_float(const char *opt, float f);

void config_get_strncpy(char* dest, size_t size, const char *opt, const char * _default);
void config_set_str(const char *opt, const char * str);

int config_get_bool(const char *opt, int _default);
void config_set_bool(const char *opt, int i);
