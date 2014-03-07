# Defines Media SDK targets to build. Should be included
# in all Android.mk files which define build targets prior
# to any other directives.

# Build HW Media SDK library
ifeq ($(MFX_IMPL_HW),)
  MFX_IMPL_HW:=true
endif

# Build SW Media SDK library
ifeq ($(MFX_IMPL_SW),)
  MFX_IMPL_SW:=true
endif

# Build HW OMX plugins
ifeq ($(MFX_OMX_IMPL_HW),)
  MFX_OMX_IMPL_HW:=true
endif

# Build SW OMX plugins
ifeq ($(MFX_OMX_IMPL_SW),)
  MFX_OMX_IMPL_SW:=true
endif

# Build OMX plugins with PAVP support
ifeq ($(MFX_OMX_PAVP),)
    MFX_OMX_PAVP:=false
endif