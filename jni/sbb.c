#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static JNIEnv *env;
static jobject curr_sbb;

static jclass cl_system;
static jclass cl_printstream;
static jclass cl_string;
static jclass cl_busyboxide;
static jclass cl_inputstream;

static jmethodID mid_println;
static jmethodID mid_is_read;
static jmethodID mid_sbb_update_status;
static jmethodID mid_sbb_display_path;
static jmethodID mid_sbb_get_version;

static jfieldID fid_out;

static int
jni_init(JNIEnv *env_, jobject this)
{
	env = env_;
	curr_sbb = this;
#define CLASS(var, id) \
	cl_##var = (*env)->FindClass(env, id); \
	if (!cl_##var) return 0;
#define METHOD(var, mycl, id, sig) \
	mid_##var = (*env)->GetMethodID(env, cl_##mycl, id, sig); \
	if (!mid_##var) return 0;
#define FIELD(var, mycl, id, sig) \
	fid_##var = (*env)->GetFieldID(env, cl_##mycl, id, sig); \
	if (!fid_##var) return 0;
#define STFIELD(var, mycl, id, sig) \
	fid_##var = (*env)->GetStaticFieldID(env, cl_##mycl, id, sig); \
	if (!fid_##var) return 0;

	CLASS(system, "java/lang/System")
	CLASS(printstream, "java/io/PrintStream")
	CLASS(string, "java/lang/String")
	CLASS(inputstream, "java/io/InputStream")
	CLASS(busyboxide, "net/yinlight/busybox/BusyBoxIDE")

	METHOD(println, printstream, "println", "(Ljava/lang/String;)V")
	METHOD(is_read, inputstream, "read", "([B)I")
	METHOD(sbb_update_status, busyboxide, "update_status",
			"(Ljava/lang/String;)V")
	METHOD(sbb_display_path, busyboxide, "display_path", "()V");
	METHOD(sbb_get_version, busyboxide, "get_version", "()I")

	STFIELD(out, system, "out", "Ljava/io/PrintStream;")

	return 1;
}

static void
println(const char *fmt, ...)
{
	char buf[1000];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	jstring *x = (*env)->NewStringUTF(env, buf);
	if (!x) return;
	jobject out = (*env)->GetStaticObjectField(env, cl_system, fid_out);
	if (!out) return;
	(*env)->CallVoidMethod(env, out, mid_println, x);
	(*env)->DeleteLocalRef(env, out);
	(*env)->DeleteLocalRef(env, x);
}

static void
update_status(char *s)
{
	jstring *x = (*env)->NewStringUTF(env, s);
	if (!x) return;
	(*env)->CallVoidMethod(env, curr_sbb, mid_sbb_update_status, x);
	(*env)->DeleteLocalRef(env, x);
}

static void
display_path(void)
{
	(*env)->CallVoidMethod(env, curr_sbb, mid_sbb_display_path);
}

static int
get_version(void)
{
	return (*env)->CallIntMethod(env, curr_sbb, mid_sbb_get_version);
}

static char *
string_from_java(jstring s)
{
	const char *t = (*env)->GetStringUTFChars(env, s, NULL);
	char *ret;
	if (!t) return NULL;
	ret = strdup(t);
	(*env)->ReleaseStringUTFChars(env, s, t);
	return ret;
}

static char *
busybox_fn(char *p)
{
	int len = strlen(p);
	char *ret = malloc(len+40);
	sprintf(ret, "%s/busybox", p);
	return ret;
}

static char *
busybox_verfn(char *p)
{
	int len = strlen(p);
	char *ret = malloc(len+40);
	sprintf(ret, "%s/busybox_version", p);
	return ret;
}

static int
check_present(char *p)
{
	return (access(busybox_fn(p), R_OK|X_OK) == 0);
}

static int
check_version(char *p, int expected)
{
	int fd;
	char buf[100];
	int i;

	fd = open(busybox_verfn(p), O_RDONLY);
	if (fd < 0) {
		return 0;
	}
	i = read(fd, buf, (sizeof buf)-1);
	close(fd);
	if (i <= 0) {
		return 0;
	}
	buf[i] = 0;
	return (strtoul(buf, NULL, 10) == expected);
}

static void
do_write(int fd, char *buf, int len)
{
	while (len > 0) {
		int i = write(fd, buf, len);
		if (i <= 0) {
			/* ugh! */
			return;
		}
		buf += i;
		len -= i;
	}
}

static int
write_busybox(char *fn, jobject is)
{
	int fd;
	jbyteArray buf;

	unlink(fn);

	fd = open(fn, O_WRONLY|O_CREAT, 00755);
	if (fd < 0) {
		char *err = strerror(errno);
		char *t = malloc(strlen(err) + strlen(fn) + 100);
		sprintf(t, "Failed to create '%s':\n%s", fn, err);
		update_status(t);
		return 0;
	}

	buf = (*env)->NewByteArray(env, 4096);
	if (!buf) {
		update_status("failed to allocate byte[4096]");
		close(fd);
		return 0;
	}

	while (1) {
		int i = (*env)->CallIntMethod(env, is, mid_is_read, buf);
		jbyte *t;
		if (i <= 0) {
			break;
		}
		t = (*env)->GetByteArrayElements(env, buf, NULL);
		do_write(fd, (char*)t, i);
		(*env)->ReleaseByteArrayElements(env, buf, t, JNI_ABORT);
	}

	fchmod(fd, 00755);
	close(fd);

	(*env)->DeleteLocalRef(env, buf);
	return 1;
}

/* use the busybox --install command to make the links for us */
static int
make_links(char *path)
{
	char *fn = busybox_fn(path);
	char *cmd = malloc(strlen(fn)*2+100);
	int i;
	sprintf(cmd, "\"%s\" --install -s \"%s\"", fn, path);
	i = system(cmd);
	if (i < 0) {
		char *err = strerror(errno);
		char *buf = malloc(strlen(fn) + 100);
		sprintf(buf, "failed to open '%s': %s", fn, err);
		update_status(buf);
	} else if (i > 0) {
		char buf[100];
		sprintf(buf, "busybox --install returned %d\n", i);
	}
	return (i == 0);
}

static int
write_version(char *fn, int ver)
{
	int fd;
	char buf[40];
	unlink(fn);
	fd = open(fn, O_WRONLY|O_CREAT, 00644);
	if (fd < 0) {
		char *err = strerror(errno);
		char *t = malloc(strlen(fn) + 100);
		sprintf(t, "failed to open '%s': %s", fn, err);
		update_status(t);
		return 0;
	}
	sprintf(buf, "%d\n", ver);
	do_write(fd, buf, strlen(buf));
	fchmod(fd, 00644);
	close(fd);
	return 1;
}

/* chmod 755 the directory itself, so the user can do an ls */
static int
share_directory(char *fn)
{
	/* NB - Oreo seems to prefer fchmod() to chmod() */
	int fd = open(fn, O_RDONLY);
	if (fd >= 0) {
		fchmod(fd, 00755);
		close(fd);
	}
	return 1;		/* always pretend to succeed */
}

JNIEXPORT void JNICALL
Java_net_yinlight_busybox_BusyBoxIDE_determine_1status(JNIEnv *env_,
		jobject this, jstring path)
{
	char *p;

	if (!jni_init(env_, this)) {
		return;
	}

	p = string_from_java(path);
	if (!check_present(p)) {
		update_status("Not unpacked.");
	} else if (!check_version(p, get_version())) {
		update_status("Wrong version is unpacked.");
	} else {
		char *buf = malloc(strlen(p)+100);
		sprintf(buf, "Already unpacked into:\n%s", p);
		update_status(buf);
		display_path();
	}
}

/* is = InputStream */
JNIEXPORT void JNICALL
Java_net_yinlight_busybox_BusyBoxIDE_native_1install(JNIEnv *env_,
	jobject this, jstring path, jobject is)
{
	char *p;

	if (!jni_init(env_, this)) {
		return;
	}

	p = string_from_java(path);

	if (write_busybox(busybox_fn(p), is) &&
	    make_links(p) &&
	    write_version(busybox_verfn(p), get_version()) &&
	    share_directory(p)) {
		char *buf = malloc(strlen(p) + 100);
		sprintf(buf, "Unpacked busybox into:\n%s", p);
		update_status(buf);
		display_path();
	}
}
