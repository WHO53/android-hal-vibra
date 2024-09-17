#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gbinder.h>

#define BINDER_VIBRATOR_DEFAULT_HIDL_DEVICE "/dev/hwbinder"
#define BINDER_VIBRATOR_DEFAULT_AIDL_DEVICE "/dev/binder"

#define BINDER_VIBRATOR_HIDL_IFACE "android.hardware.vibrator@1.0::IVibrator"
#define BINDER_VIBRATOR_AIDL_IFACE "android.hardware.vibrator.IVibrator"
#define BINDER_VIBRATOR_AIDL_CALLBACK_IFACE "android.hardware.vibrator.IVibratorCallback"
#define BINDER_VIBRATOR_HIDL_SLOT "default"
#define BINDER_VIBRATOR_AIDL_SLOT "default"

enum {
    BINDER_VIBRATOR_HIDL_1_0_ON = 1,
    BINDER_VIBRATOR_HIDL_1_0_OFF = 2,
    BINDER_VIBRATOR_AIDL_ON = 3,
    BINDER_VIBRATOR_AIDL_OFF = 2,
};

typedef enum {
    VIBRATOR_TYPE_NONE,
    VIBRATOR_TYPE_HIDL,
    VIBRATOR_TYPE_AIDL
} VibratorType;

static GBinderServiceManager *service_manager = NULL;
static GBinderRemoteObject *remote = NULL;
static GBinderClient *client = NULL;
static GBinderLocalObject *callback_object = NULL;
static VibratorType vibrator_type = VIBRATOR_TYPE_NONE;

static GBinderLocalReply *
vibrator_callback(GBinderLocalObject *obj,
                  GBinderRemoteRequest *req,
                  guint code,
                  guint flags,
                  int *status,
                  void *user_data)
{
    return NULL;
}

static int init_vibrator() {
    service_manager = gbinder_servicemanager_new(BINDER_VIBRATOR_DEFAULT_HIDL_DEVICE);
    if (service_manager) {
        remote = gbinder_servicemanager_get_service_sync(service_manager, 
                                                         BINDER_VIBRATOR_HIDL_IFACE "/" BINDER_VIBRATOR_HIDL_SLOT, 
                                                         NULL);
        if (remote) {
            client = gbinder_client_new(remote, BINDER_VIBRATOR_HIDL_IFACE);
            if (client) {
                vibrator_type = VIBRATOR_TYPE_HIDL;
                printf("Using HIDL vibrator interface\n");
                return 1;
            }
        }
    }

    if (service_manager) gbinder_servicemanager_unref(service_manager);
    if (remote) gbinder_remote_object_unref(remote);

    service_manager = gbinder_servicemanager_new(BINDER_VIBRATOR_DEFAULT_AIDL_DEVICE);
    if (service_manager) {
        remote = gbinder_servicemanager_get_service_sync(service_manager, 
                                                         BINDER_VIBRATOR_AIDL_IFACE "/" BINDER_VIBRATOR_AIDL_SLOT, 
                                                         NULL);
        if (remote) {
            client = gbinder_client_new(remote, BINDER_VIBRATOR_AIDL_IFACE);
            if (client) {
                callback_object = gbinder_servicemanager_new_local_object(service_manager,
                                                                          BINDER_VIBRATOR_AIDL_CALLBACK_IFACE,
                                                                          vibrator_callback,
                                                                          NULL);
                vibrator_type = VIBRATOR_TYPE_AIDL;
                printf("Using AIDL vibrator interface\n");
                return 1;
            }
        }
    }

    fprintf(stderr, "Failed to initialize vibrator\n");
    return 0;
}

static void cleanup_vibrator() {
    if (callback_object) gbinder_local_object_unref(callback_object);
    if (remote) gbinder_remote_object_unref(remote);
    if (service_manager) gbinder_servicemanager_unref(service_manager);
}

static int vibrate(int duration_ms) {
    GBinderLocalRequest *req = gbinder_client_new_request(client);
    GBinderRemoteReply *reply;
    int status;

    if (vibrator_type == VIBRATOR_TYPE_HIDL) {
        gbinder_local_request_append_int32(req, duration_ms);
        reply = gbinder_client_transact_sync_reply(client, BINDER_VIBRATOR_HIDL_1_0_ON, req, &status);
    } else if (vibrator_type == VIBRATOR_TYPE_AIDL) {
        gbinder_local_request_append_int32(req, duration_ms);
        gbinder_local_request_append_local_object(req, callback_object);
        reply = gbinder_client_transact_sync_reply(client, BINDER_VIBRATOR_AIDL_ON, req, &status);
    } else {
        fprintf(stderr, "Unknown vibrator type\n");
        gbinder_local_request_unref(req);
        return 0;
    }

    gbinder_local_request_unref(req);

    if (status == GBINDER_STATUS_OK) {
        int32_t result;
        gbinder_remote_reply_read_int32(reply, &result);
        gbinder_remote_reply_unref(reply);
        return (result == 0);
    } else {
        fprintf(stderr, "Failed to turn on vibrator\n");
        return 0;
    }
}

static int stop_vibrate() {
    GBinderLocalRequest *req = gbinder_client_new_request(client);
    GBinderRemoteReply *reply;
    int status;

    if (vibrator_type == VIBRATOR_TYPE_HIDL) {
        reply = gbinder_client_transact_sync_reply(client, BINDER_VIBRATOR_HIDL_1_0_OFF, req, &status);
    } else if (vibrator_type == VIBRATOR_TYPE_AIDL) {
        reply = gbinder_client_transact_sync_reply(client, BINDER_VIBRATOR_AIDL_OFF, req, &status);
    } else {
        fprintf(stderr, "Unknown vibrator type\n");
        gbinder_local_request_unref(req);
        return 0;
    }

    gbinder_local_request_unref(req);

    if (status == GBINDER_STATUS_OK) {
        int32_t result;
        gbinder_remote_reply_read_int32(reply, &result);
        gbinder_remote_reply_unref(reply);
        return (result == 0);
    } else {
        fprintf(stderr, "Failed to turn off vibrator\n");
        return 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <duration_ms>\n", argv[0]);
        return 1;
    }

    int duration_ms = atoi(argv[1]);
    if (duration_ms <= 0) {
        fprintf(stderr, "Duration must be a positive integer\n");
        return 1;
    }

    if (!init_vibrator()) {
        fprintf(stderr, "Failed to initialize vibrator\n");
        return 1;
    }

    if (vibrate(duration_ms)) {
        printf("Vibrating for %d ms\n", duration_ms);
        g_usleep(duration_ms * 1000);
        if (stop_vibrate()) {
            printf("Vibration stopped\n");
        } else {
            fprintf(stderr, "Failed to stop vibration\n");
        }
    } else {
        fprintf(stderr, "Failed to start vibration\n");
    }

    cleanup_vibrator();
    return 0;
}
