/*
 * object.c
 * Handle create and subsequent garbage collection of objects.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 */

#define	ADBG(s)

#include "config.h"
#include "debug.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "lookup.h"
#include "itypes.h"
#include "baseClasses.h"
#include "errors.h"
#include "exception.h"
#include "itypes.h"
#include "md.h"
#include "external.h"
#include "gc.h"
#include "thread.h"
#include "jvmpi_kaffe.h"

Hjava_lang_Object*
newObjectChecked(Hjava_lang_Class* class, errorInfo *info)
{
	Hjava_lang_Object* obj;

	if (CLASS_IS_INTERFACE(class)) {
		postExceptionMessage(info,
				     JAVA_LANG(InstantiationError),
				     "(class: %s) "
				     "Abstract class.",
				     CLASS_CNAME(class));
		return (0);
	}
	obj = gc_malloc(CLASS_FSIZE(class), class->alloc_type);

	if (!obj) {
		postOutOfMemory(info);
	} else {
	    /* Fill in object information */
	    obj->dtable = class->dtable;

#if defined(ENABLE_JVMPI)
	    if( JVMPI_EVENT_ISENABLED(JVMPI_EVENT_OBJECT_ALLOC) )
	    {
		    JVMPI_Event ev;
		    
		    ev.event_type = JVMPI_EVENT_OBJECT_ALLOC;
		    ev.u.obj_alloc.arena_id = -1;
		    ev.u.obj_alloc.class_id = class;
		    ev.u.obj_alloc.is_array = JVMPI_NORMAL_OBJECT;
		    ev.u.obj_alloc.size = CLASS_FSIZE(class);
		    ev.u.obj_alloc.obj_id = obj;
		    jvmpiPostEvent(&ev);
	    }
#endif
	    
	}
DBG(NEWOBJECT,
	dprintf("newObject %p class %s\n", obj,
		(class ? class->name->data : "<none>"));
    )
        return (obj);
}
/*
 * Create a new Java object.
 *
 * Throws an out of memory exception if the object cannot be allocated.
 * NB: newObject must be called in user mode.
 */
Hjava_lang_Object*
newObject(Hjava_lang_Class* class)
{
	errorInfo info;
	Hjava_lang_Object *obj = newObjectChecked(class, &info);

	if (obj == 0) {
		throwError(&info);
	}

        return (obj);
}

/*
 * Allocate a new class object.
 * We make Class objects roots if class gc is disabled.
 *
 * Returns null if the class object cannot be allocated because
 * of out of memory.
 */
Hjava_lang_Class*
newClass(void)
{
	Hjava_lang_Class* cls;

	cls = gc_malloc(sizeof(Hjava_lang_Class), GC_ALLOC_CLASSOBJECT);
	if (cls == 0) {
		goto done;
	}

	/* Turn off class gc */
	if (Kaffe_JavaVMArgs[0].enableClassGC == 0) {
		if (!gc_add_ref(cls)) {
			return 0;
		}
	}

        /* Fill in object information */
	cls->head.dtable = ClassClass->dtable;
done:
DBG(NEWOBJECT,
	dprintf("newClass @%p\n", cls);
    )

        return (cls);
}

/*
 * Allocate a new array, of whatever types.
 */
Hjava_lang_Object*
newArrayChecked(Hjava_lang_Class* elclass, int count, errorInfo *info)
{
	Hjava_lang_Class* class = 0;
	Hjava_lang_Object* obj = 0;

	if (CLASS_IS_PRIMITIVE(elclass) || elclass == PtrClass) {
		size_t total_count = (TYPE_SIZE(elclass) * count) + ARRAY_DATA_OFFSET;
		if (total_count > count) {
			obj = gc_malloc(total_count, GC_ALLOC_PRIMARRAY);
		}
	}
	else {
		size_t total_count = (PTR_TYPE_SIZE * count) + ARRAY_DATA_OFFSET;
		if (total_count > count) {
			obj = gc_malloc(total_count, GC_ALLOC_REFARRAY);
		}
	}
	if (obj) {
		class = lookupArray(elclass, info);
	} else {
		postOutOfMemory(info);
	}

	if (class) {
		obj->dtable = class->dtable;
		ARRAY_SIZE(obj) = count;
	}
DBG(NEWOBJECT,
	dprintf("newArray %p class %s count %d\n", obj,
		(class ? class->name->data : "<none>"), count);
    )
	return (obj);
}

/*
 * Allocate a new array, of whatever types.
 */
Hjava_lang_Object*
newArray(Hjava_lang_Class* elclass, int count)
{
	Hjava_lang_Object* obj;
	errorInfo info;

	obj = newArrayChecked(elclass, count, &info);
	if (!obj) {
		throwError(&info);
	}
	return (obj);
}

/*
 * Allocate a new multi-dimensional array.
 */
Hjava_lang_Object*
newMultiArrayChecked(Hjava_lang_Class* clazz, int* dims, errorInfo *einfo)
{
	Hjava_lang_Object* obj;
	Hjava_lang_Object** array;
	int i;

	obj = newArrayChecked(CLASS_ELEMENT_TYPE(clazz), dims[0], einfo);
	if (!obj) {
	    return NULL;
	}

	if (dims[1] >= 0) {
		array = OBJARRAY_DATA(obj);
		for (i = 0; i < dims[0]; i++) {
			array[i] = newMultiArrayChecked(CLASS_ELEMENT_TYPE(clazz), &dims[1], einfo);
			if (!array[i]) {
			    return NULL;
			}
		}
	}

	return (obj);
}

/*
 * Allocate a new multi-dimensional array.
 */
Hjava_lang_Object*
newMultiArray(Hjava_lang_Class* clazz, int* dims)
{
	Hjava_lang_Object* obj;
	errorInfo einfo;

	obj = newMultiArrayChecked(clazz, dims, &einfo);
	if (!obj) {
		throwError(&einfo);
	}
	return (obj);
}

