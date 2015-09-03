/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef LISTENER_WAYLAND_H
#define LISTENER_WAYLAND_H

#include "class_wayland.h"

/* drm listeners */
void drm_handle_device(void *data
    , struct wl_drm *drm
    , const char *device);

void drm_handle_format(void *data
    , struct wl_drm *drm
    , uint32_t format);

void drm_handle_authenticated(void *data
    , struct wl_drm *drm);

void drm_handle_capabilities(void *data
    , struct wl_drm *drm
    , uint32_t value);

/* registry listeners */
void registry_handle_global(void *data
    , struct wl_registry *registry
    , uint32_t name
    , const char *interface
    , uint32_t version);

void remove_registry_global(void *data
    , struct wl_registry *regsitry
    , uint32_t name);

/* surface listener */
void shell_surface_ping(void *data
    , struct wl_shell_surface *shell_surface
    , uint32_t serial);

void shell_surface_configure(void *data
    , struct wl_shell_surface *shell_surface
    , uint32_t edges
    , int32_t width
    , int32_t height);

void handle_done(void *data, struct wl_callback *callback, uint32_t time);

void buffer_release(void *data, struct wl_buffer *buffer);
#endif /* LISTENER_WAYLAND_H */
