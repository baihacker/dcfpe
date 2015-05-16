#ifndef DPE_BASE_DPE_BASE_EXPORT_H_
#define DPE_BASE_DPE_BASE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DPE_BASE_IMPLEMENTATION)
#define DPE_BASE_EXPORT __declspec(dllexport)
#define DPE_BASE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define DPE_BASE_EXPORT __declspec(dllimport)
#define DPE_BASE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DPE_BASE_IMPLEMENTATION)
#define DPE_BASE_EXPORT __attribute__((visibility("default")))
#define DPE_BASE_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define DPE_BASE_EXPORT
#define DPE_BASE_EXPORT_PRIVATE
#endif  // defined(BASE_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define DPE_BASE_EXPORT
#define DPE_BASE_EXPORT_PRIVATE
#endif

#endif  // BASE_DPE_BASE_EXPORT_H_
