/*
 * support.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#define	DBG(s)
#define	FDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include "jtypes.h"
#include "jsyscall.h"
#include "jmalloc.h"
#include "gtypes.h"
#include "gc.h"
#include "constants.h"
#include "file.h"
#include "files.h"
#include "access.h"
#include "readClassConfig.h"
#include "readClass.h"
#include "jar.h"
#include "kaffeh-support.h"

#if !defined(S_ISDIR)
#define	S_ISDIR(m)	((m) & S_IFDIR)
#endif

#if defined(__WIN32__) || defined (__amigaos__)
#define	PATHSEP	';'
#else
#define	PATHSEP	':'
#endif

void findClass(char*);

extern char realClassPath[];
extern char className[];
extern FILE* include;
extern FILE* jni_include;
extern int flag_jni;

extern char* translateSig(char*, char**, int*);
extern char* translateSigType(char*, char*);

static int objectDepth = -1;
static int argpos = 0;

struct _Collector;
static void* gcMalloc(struct _Collector*, size_t, int);
static void* gcRealloc(struct _Collector*, void*, size_t, int);
static void  gcFree(struct _Collector*, void*);

static inline int
binary_open(const char *file, int mode, int perm, int *);

static int 
kread(int fd, void *buf, size_t len, ssize_t *out)
{
	*out = read(fd, buf, len);
	return (*out == -1) ? errno : 0;
}

static int 
kwrite(int fd, const void *buf, size_t len, ssize_t *out)
{
	*out = write(fd, buf, len);
	return (*out == -1) ? errno : 0;
}

static int
klseek(int fd, off_t off, int whence, off_t *out)
{
	*out = lseek(fd, off, whence);
	return (*out == -1) ? errno : 0;
}

/*
 * We use a very simple 'fake' threads subsystem
 */

SystemCallInterface Kaffe_SystemCallInterface =
{
	binary_open,
        kread,
        kwrite, 
        klseek,
        close,
        fstat,
        stat,

        NULL,		/* mkdir */
        NULL,		/* rmdir */
        NULL,		/* rename */
        NULL,		/* remove */
        NULL,		/* socket */
        NULL,		/* connect */
        NULL,		/* bind */
        NULL,		/* listen */
        NULL,		/* accept */
        NULL,		/* sockread */
        NULL,		/* recvfrom */
        NULL,		/* sockwrite */
        NULL,		/* sendto */
        NULL,		/* setsockopt */
        NULL,		/* getsockopt */
        NULL,		/* getsockname */
        NULL,		/* getpeername */
        NULL,		/* sockclose */
        NULL,		/* gethostbyname */
        NULL,		/* gethostbyaddr */
        NULL,		/* select */
        NULL,		/* forkexec */
        NULL,		/* waitpid */
        NULL,		/* kill */
};

/*
 * We use a very simple 'fake' garbage collector interface
 */

struct GarbageCollectorInterface_Ops GC_Ops = {
	NULL,
	NULL,
	NULL,
	gcMalloc,
	gcRealloc,
	gcFree,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

struct _Collector c = { & GC_Ops }, *main_collector = &c;

/* 
 * Ensure that files are opened in binary mode; the MS-Windows port
 * depends on this.
 */
static inline int
binary_open(const char *file, int mode, int perm, int *out) {
  *out = open(file, mode | O_BINARY, perm);
  return *out == -1 ? errno : 0;
}

/*
 * Init include file.
 */
void
initInclude(void)
{
	argpos = 0;

	if (include == 0) {
		return;
	}

	fprintf(include, "/* DO NOT EDIT THIS FILE - it is machine generated */\n");
	fprintf(include, "#include <native.h>\n");
	fprintf(include, "\n");
	fprintf(include, "#ifndef _Included_%s\n", className);
	fprintf(include, "#define _Included_%s\n", className);
	fprintf(include, "\n");
	fprintf(include, "#ifdef __cplusplus\n");
	fprintf(include, "extern \"C\" {\n");
	fprintf(include, "#endif\n");
}

/*
 * Start include file.
 */
void
startInclude(void)
{
	argpos = 0;

	if (include == 0) {
		return;
	}

	fprintf(include, "\n");
	fprintf(include, "/* Header for class %s */\n", className);
	fprintf(include, "\n");
	fprintf(include, "typedef struct H%s {\n", className);
	fprintf(include, "  Hjava_lang_Object base;\n");
}

/*
 * End include file.
 */
void
endInclude(void)
{
	if (include == 0) {
		return;
	}

	fprintf(include, "\n");
	fprintf(include, "#ifdef __cplusplus\n");
	fprintf(include, "}\n");
	fprintf(include, "#endif\n");
	fprintf(include, "\n");
	fprintf(include, "#endif\n");
}

void
initJniInclude(void)
{
	if (jni_include == 0) {
		return;
	}

	fprintf(jni_include, "/* DO NOT EDIT THIS FILE - it is machine generated */\n");
	fprintf(jni_include, "#include <jni.h>\n");
	fprintf(jni_include, "\n");
	fprintf(jni_include, "#ifndef _Included_%s\n", className);
	fprintf(jni_include, "#define _Included_%s\n", className);
	fprintf(jni_include, "\n");
	fprintf(jni_include, "#ifdef __cplusplus\n");
	fprintf(jni_include, "extern \"C\" {\n");
	fprintf(jni_include, "#endif\n");
	fprintf(jni_include, "\n");
}

void
startJniInclude(void)
{
}

void
endJniInclude(void)
{
	if (jni_include == 0) {
		return;
	}

	fprintf(jni_include, "\n");
	fprintf(jni_include, "#ifdef __cplusplus\n");
	fprintf(jni_include, "}\n");
	fprintf(jni_include, "#endif\n");
	fprintf(jni_include, "\n");
	fprintf(jni_include, "#endif\n");
}


/*
 * Add a class.
 */
void
addClass(u2 this, u2 super, u2 access, constants* cpool)
{
	if (super == 0) {
		return;
	}

	findClass((char*)cpool->data[cpool->data[super]]);
}

/*
 * Finish processing a class's fields.
 */
void
readFieldEnd(void)
{
	if (include == 0) {
		return;
	}

	if (objectDepth == 0) {
#if 0
		if (argpos == 0) {
			fprintf(include, "\tint __DUMMY__;\n");
		}
#endif
		fprintf(include, "} H%s;\n\n", className);
	}
}

/*
 * Read and process a class field.
 */
void
readField(classFile* fp, Hjava_lang_Class* this, constants* cpool)
{
	field_info f;
	int argsize = 0;
	char* arg;
	int i;
	u2 cnt;
	u2 idx;
	u4 len;
	char cval[1000];
	union { jint i[2]; jdouble d; } u;

	readu2(&f.access_flags, fp);
	readu2(&f.name_index, fp);
	readu2(&f.signature_index, fp);

	/* Read in attributes */
	cval[0] = 0;
	readu2(&cnt, fp);
	for (i = 0; i < cnt; i++) {
		readu2(&idx, fp);
		readu4(&len, fp);
		if (strcmp((char*)cpool->data[idx], "ConstantValue") == 0) {
			assert(len == 2);
			readu2(&idx, fp);
			switch (cpool->tags[idx]) {
			case CONSTANT_Integer:
				sprintf(cval, "%d", (int)cpool->data[idx]);
				break;
			case CONSTANT_Float:
				sprintf(cval, "%.7e", *(float*)&cpool->data[idx]);
				break;
			case CONSTANT_Long:
#if SIZEOF_VOIDP == 8
				sprintf(cval, "0x%016lx", cpool->data[idx]);
#else
#if defined(WORDS_BIGENDIAN)
				sprintf(cval, "0x%08x%08x", cpool->data[idx], cpool->data[idx+1]);
#else
				sprintf(cval, "0x%08x%08x", cpool->data[idx+1], cpool->data[idx]);
#endif
#endif
				break;
			case CONSTANT_Double:
				u.i[0] = cpool->data[idx];
				u.i[1] = cpool->data[idx + 1];
				sprintf(cval, "%.16e", u.d);
				break;
			case CONSTANT_String:
				sprintf(cval, "\"%s\"", (char*)cpool->data[cpool->data[idx]]);
				break;
			default:
				sprintf(cval, "?unsupported type?");
				break;
			}
		}
		else {
			seekm(fp, len);
		}
	}

	if (include != 0) {

		arg = translateSig((char*)cpool->data[f.signature_index], 0, &argsize);
		if (f.access_flags & ACC_STATIC) {
			if ((f.access_flags & (ACC_PUBLIC|ACC_FINAL)) == (ACC_PUBLIC|ACC_FINAL) && cval[0] != 0) {
				fprintf(include, "#define %s_%s %s\n", className, (char*)cpool->data[f.name_index], cval);
			}
		}
		else {
			argpos += argsize;
			fprintf(include, "  %s %s;\n", arg, (char*)cpool->data[f.name_index]);
		}
FDBG(		printf("Field: %s %s\n", arg, (char*)cpool->data[f.name_index]);			)
	}
}

/*
 * Read and process a method.
 */
void
readMethod(classFile* fp, Hjava_lang_Class* this, constants* cpool)
{
	method_info m;
	char* name;
	char* sig;
	char* str;
	char* ret;
	char* tsig;
	int args;

	readu2(&m.access_flags, fp);
	readu2(&m.name_index, fp);
	readu2(&m.signature_index, fp);

	/* If we shouldn't generate method prototypes, quit now */
	if (objectDepth > 0) {
		return;
	}

DBG(	printf("Method %s%s\n", (char*)cpool->data[m.name_index], (char*)cpool->data[m.signature_index]);	)

	/* Only generate stubs for native methods */
	if (!(m.access_flags & ACC_NATIVE)) {
		return;
	}
	args = 0;

	/* Generate method prototype */
	name = (char*)cpool->data[m.name_index];
	sig = (char*)cpool->data[m.signature_index];
	ret = strchr(sig,')');
	ret++;

	if (include != 0) {
		fprintf(include, "extern %s", translateSig(ret, 0, 0));
		fprintf(include, " %s_%s(", className, name);
		if (!(m.access_flags & ACC_STATIC)) {
			fprintf(include, "struct H%s*", className);
			if (sig[1] != ')') {
				fprintf(include, ", ");
			}
		} else if (sig[1] == ')') {
			fprintf(include, "void");
		}
	}

	if (jni_include != 0) {
		fprintf(jni_include, "JNIEXPORT ");
		switch (*ret) {
		case '[':
			fprintf(jni_include, "jarray");
			break;
		case 'L':
			if (strncmp(ret, "Ljava/lang/Class;", 17) == 0) {
				fprintf(jni_include, "jclass");
			}
			else if (strncmp(ret, "Ljava/lang/String;", 18) == 0) {
				fprintf(jni_include, "jstring");
			}
			else {
				fprintf(jni_include, "jobject");
			}
			break;
		case 'I':
			fprintf(jni_include, "jint");
			break;
		case 'Z':
			fprintf(jni_include, "jboolean");
			break;
		case 'S':
			fprintf(jni_include, "jshort");
			break;
		case 'B':
			fprintf(jni_include, "jbyte");
			break;
		case 'C':
			fprintf(jni_include, "jchar");
			break;
		case 'F':
			fprintf(jni_include, "jfloat");
			break;
		case 'J':
			fprintf(jni_include, "jlong");
			break;
		case 'D':
			fprintf(jni_include, "jdouble");
			break;
		case 'V':
			fprintf(jni_include, "void");
			break;
		default:
			fprintf(stderr, "unknown return type `%c' for "
				"method %s.%s, bailing out.\n", *ret,
				className, name);
			exit(0);
		}
		fprintf(jni_include, " JNICALL Java_%s_%s(JNIEnv*", className, name);
		if ((m.access_flags & ACC_STATIC)) {
			fprintf(jni_include, ", jclass");
		}
		else {
			fprintf(jni_include, ", jobject");
		}
	}

	str = sig + 1;
	args++;
	while (str[0] != ')') {
		if (jni_include != 0) {
			switch (str[0]) {
			case '[':
				fprintf(jni_include, ", jarray");
				break;
			case 'L':
				if (strncmp(str, "Ljava/lang/Class;", 17) == 0) {
					fprintf(jni_include, ", jclass");
				}
				else if (strncmp(str, "Ljava/lang/String;", 18) == 0) {
					fprintf(jni_include, ", jstring");
				}
				else {
					fprintf(jni_include, ", jobject");
				}
				break;
			case 'I':
				fprintf(jni_include, ", jint");
				break;
			case 'Z':
				fprintf(jni_include, ", jboolean");
				break;
			case 'S':
				fprintf(jni_include, ", jshort");
				break;
			case 'B':
				fprintf(jni_include, ", jbyte");
				break;
			case 'C':
				fprintf(jni_include, ", jchar");
				break;
			case 'F':
				fprintf(jni_include, ", jfloat");
				break;
			case 'J':
				fprintf(jni_include, ", jlong");
				break;
			case 'D':
				fprintf(jni_include, ", jdouble");
				break;
			}
		}
		tsig = translateSig(str, &str, &args);
		if (include != 0) {
			fprintf(include, "%s", tsig);
			if (str[0] != ')') {
				fprintf(include, ", ");
			}
		}
	}
	if (include != 0) {
		fprintf(include, ");\n");
	}
	if (jni_include != 0) {
		fprintf(jni_include, ");\n");
	}
}

/*
 * Locate class specified and process it.
 */
void
findClass(char* nm)
{
	int fd;
	jarFile* jfile;
	jarEntry* jentry;
	char superName[512];
	struct stat sbuf;
	char* start;
	char* end = (char*)1;
	constants* savepool;
	classFile hand;

	/* If classpath isn't set, get it from the environment */
	if (realClassPath[0] == 0) {
		start = getenv("KAFFE_CLASSPATH");
		if (start == 0) {
			start = getenv("CLASSPATH");
		}
		if (start == 0) {
			fprintf(stderr, "CLASSPATH not set!\n");
			exit(1);
		}
		strcpy(realClassPath, start);
	}

	for (start = realClassPath; end != 0; start = end + 1) {
		end = strchr(start, PATHSEP);
		if (end == 0) {
			strcpy(superName, start);
		}
		else {
			strncpy(superName, start, end-start);
			superName[end-start] = 0;
		}

		if (stat(superName, &sbuf) < 0) {
			/* Ignore */
		}
		else if (S_ISDIR(sbuf.st_mode)) {
			strcat(superName, "/");
			strcat(superName, nm);
			strcat(superName, ".class");
			fd = open(superName, O_RDONLY|O_BINARY, 0);
			if (fd < 0) {
				continue;
			}
			if (fstat(fd, &sbuf) < 0) {
				close(fd);
				continue;
			}

			hand.size = sbuf.st_size;
			hand.base = malloc(hand.size);
			hand.buf = hand.base;
			if (read(fd, hand.base, hand.size) != hand.size) {
				free(hand.base);
				close(fd);
				continue;
			}

			objectDepth++;
			savepool = constant_pool;

			readClass(NULL, &hand, NULL, NULL);

			constant_pool = savepool;
			objectDepth--;

			free(hand.base);

			close(fd);
			return;
		}
		else {
			/* JAR file */
			jfile = openJarFile(superName);
			if (jfile == 0) {
				continue;
			}

			strcpy(superName, nm);
			strcat(superName, ".class");

			jentry = lookupJarFile(jfile, superName);
			if (jentry == 0) {
				closeJarFile(jfile);
				continue;
			}

			hand.base = getDataJarFile(jfile, jentry);
			hand.size = jentry->uncompressedSize;
			hand.buf = hand.base;

			objectDepth++;
			savepool = constant_pool;

			readClass(NULL, &hand, NULL, NULL);

			constant_pool = savepool;
			objectDepth--;

			free(hand.base);

			closeJarFile(jfile);
			return;
		}
	}
	fprintf(stderr, "Failed to open object '%s'\n", nm);
	exit(1);
}

/*
 * The real GC malloc zeroes memory, so our malloc does also.
 */
void*
jmalloc(size_t sz)
{
  	void	*p;

	if (!sz) {
		++sz; /* never malloc(0), it may return NULL */
	}

	if ((p = malloc(sz)) == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	memset(p, 0, sz);
	return(p);
}

void*
jrealloc(void* mem, size_t sz)
{
	return(realloc(mem, sz));
}

void
jfree(void* mem)
{
	free(mem);
}

static void *
gcMalloc(struct _Collector *collector, size_t sz, int type)
{
	return(jmalloc(sz));
}

static void *
gcRealloc(struct _Collector *collector, void *mem, size_t sz, int type)
{
	return(jrealloc(mem, sz));
}

static void  gcFree(struct _Collector *collector, void *mem)
{
	jfree(mem);
}

void 
postExceptionMessage(struct _errorInfo *e, 
	const char *name, const char *msgfmt, ...)
{
	/* XXX */
	fprintf(stderr, "Error %s, %s\n", name, msgfmt);
}
