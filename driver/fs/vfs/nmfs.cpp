#define ALL_IMPLEMENTATION

#include <errno.h>
#include <fs/vfs/vfs.h>
#include <krlibc.h>
#include <net/socket.h>

enum {
    NMFS_KIND_ROOT = 0,
    NMFS_KIND_STATUS = 1,
    NMFS_KIND_STATE = 2,
};

typedef struct nmfs_handle {
    int kind;
} nmfs_handle_t;

static int           nmfs_id   = 0;
static vfs_node_t    nmfs_root = NULL;
static nmfs_handle_t nmfs_root_handle   = {NMFS_KIND_ROOT};
static nmfs_handle_t nmfs_status_handle = {NMFS_KIND_STATUS};
static nmfs_handle_t nmfs_state_handle  = {NMFS_KIND_STATE};

static void nmfs_format_ipv4_text(uint32_t addr, char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) return;

    uint32_t host = ((addr & 0x000000ffU) << 24) |
                    ((addr & 0x0000ff00U) << 8) |
                    ((addr & 0x00ff0000U) >> 8) |
                    ((addr & 0xff000000U) >> 24);
    snprintf(buffer, size, "%u.%u.%u.%u",
             (unsigned int)((host >> 24) & 0xffU),
             (unsigned int)((host >> 16) & 0xffU),
             (unsigned int)((host >> 8) & 0xffU),
             (unsigned int)(host & 0xffU));
}

static size_t nmfs_copy_text_out(const char *text, size_t text_len, void *addr, size_t offset, size_t size)
{
    if (text == NULL || addr == NULL || offset >= text_len) {
        return 0;
    }

    size_t remain = text_len - offset;
    if (size > remain) {
        size = remain;
    }
    memcpy(addr, text + offset, size);
    return size;
}

static const char *nmfs_state_name(const socket_netinfo_t *info)
{
    if (info == NULL || !info->stack_ready) return "starting";
    if (info->link_up && info->ip_ready) return "connected";
    if (info->link_up) return "configuring";
    return "disconnected";
}

static const char *nmfs_connectivity_name(const socket_netinfo_t *info)
{
    if (info == NULL || !info->stack_ready) return "unknown";
    return (info->link_up && info->ip_ready) ? "full" : "none";
}

static const char *nmfs_method_name(const socket_netinfo_t *info)
{
    if (info == NULL) return "disabled";

    switch (info->ipv4_method) {
    case SOCKET_IPV4_METHOD_DHCP:
        return "auto";
    case SOCKET_IPV4_METHOD_MANUAL:
        return "manual";
    default:
        return "disabled";
    }
}

static const char *nmfs_dhcp_state_name(const socket_netinfo_t *info)
{
    if (info == NULL || info->ipv4_method != SOCKET_IPV4_METHOD_DHCP) return "inactive";

    switch (info->dhcp_state) {
    case 0:
        return "off";
    case 1:
        return "requesting";
    case 2:
        return "init";
    case 3:
        return "rebooting";
    case 4:
        return "rebinding";
    case 5:
        return "renewing";
    case 6:
        return "selecting";
    case 7:
        return "informing";
    case 8:
        return "checking";
    case 9:
        return "permanent";
    case 10:
        return "bound";
    case 11:
        return "releasing";
    case 12:
        return "backing-off";
    default:
        return "unknown";
    }
}

static size_t nmfs_build_status_text(char *buffer, size_t size)
{
    socket_netinfo_t info;
    memset(&info, 0, sizeof(info));

    if (buffer == NULL || size == 0) return 0;

    if (socket_get_netinfo(&info) < 0) {
        return (size_t)snprintf(buffer, size,
                                "[device]\nmanaged=true\nstate=unavailable\n\n"
                                "[connectivity]\nstate=unknown\n");
    }

    char ip_text[32]      = "unassigned";
    char mask_text[32]    = "unassigned";
    char gateway_text[32] = "unassigned";
    char dns_text[32]     = "unassigned";
    if (info.ip_ready) {
        nmfs_format_ipv4_text(info.ipv4_addr, ip_text, sizeof(ip_text));
        nmfs_format_ipv4_text(info.ipv4_netmask, mask_text, sizeof(mask_text));
        nmfs_format_ipv4_text(info.ipv4_gateway, gateway_text, sizeof(gateway_text));
    }
    if (info.dns_server != 0) {
        nmfs_format_ipv4_text(info.dns_server, dns_text, sizeof(dns_text));
    }

    const char *ifname     = info.ifname[0] != '\0' ? info.ifname : "e0";
    const char *method     = nmfs_method_name(&info);
    const char *dhcp_state = nmfs_dhcp_state_name(&info);

    return (size_t)snprintf(
        buffer, size,
        "[device]\n"
        "managed=true\n"
        "interface=%s\n"
        "state=%s\n"
        "carrier=%s\n"
        "mac=%02x:%02x:%02x:%02x:%02x:%02x\n"
        "mtu=%u\n"
        "\n"
        "[ipv4]\n"
        "method=%s\n"
        "dhcp-state=%s\n"
        "address=%s\n"
        "netmask=%s\n"
        "gateway=%s\n"
        "dns=%s\n"
        "\n"
        "[connectivity]\n"
        "state=%s\n",
        ifname,
        nmfs_state_name(&info),
        info.link_up ? "true" : "false",
        info.mac[0], info.mac[1], info.mac[2], info.mac[3], info.mac[4], info.mac[5],
        (unsigned int)info.mtu,
        method,
        dhcp_state,
        ip_text,
        mask_text,
        gateway_text,
        dns_text,
        nmfs_connectivity_name(&info));
}

static size_t nmfs_build_state_text(char *buffer, size_t size)
{
    socket_netinfo_t info;
    memset(&info, 0, sizeof(info));

    if (buffer == NULL || size == 0) return 0;

    if (socket_get_netinfo(&info) < 0) {
        return (size_t)snprintf(buffer, size, "state=unavailable\n");
    }

    return (size_t)snprintf(buffer, size,
                            "state=%s\nconnectivity=%s\nmanaged=true\n",
                            nmfs_state_name(&info), nmfs_connectivity_name(&info));
}

static errno_t nmfs_mount(const char *handle, vfs_node_t node)
{
    if ((uint64_t)handle != NM_REGISTER_ID) {
        return VFS_STATUS_FAILED;
    }

    node->fsid   = nmfs_id;
    node->type   = file_dir;
    node->handle = &nmfs_root_handle;
    nmfs_root    = node;

    vfs_node_t status_node = vfs_child_append(node, "status", &nmfs_status_handle);
    if (status_node != NULL) {
        status_node->type = file_stream;
        status_node->size = (uint64_t)-1;
    }

    vfs_node_t state_node = vfs_child_append(node, "state", &nmfs_state_handle);
    if (state_node != NULL) {
        state_node->type = file_stream;
        state_node->size = (uint64_t)-1;
    }

    return VFS_STATUS_SUCCESS;
}

static void nmfs_open(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    if (node == nmfs_root) {
        node->handle = &nmfs_root_handle;
        node->type   = file_dir;
        node->size   = 0;
    } else if (name != NULL && strcmp(name, "status") == 0) {
        node->handle = &nmfs_status_handle;
        node->type   = file_stream;
        node->size   = (uint64_t)-1;
    } else if (name != NULL && strcmp(name, "state") == 0) {
        node->handle = &nmfs_state_handle;
        node->type   = file_stream;
        node->size   = (uint64_t)-1;
    } else {
        node->type = file_stream;
        node->size = (uint64_t)-1;
    }
}

static errno_t nmfs_stat(void *file, vfs_node_t node)
{
    nmfs_handle_t *handle = (nmfs_handle_t *)file;
    if (handle == NULL) {
        return VFS_STATUS_FAILED;
    }

    node->fsid = nmfs_id;
    if (handle->kind == NMFS_KIND_ROOT) {
        node->type = file_dir;
        node->size = 0;
    } else {
        node->type = file_stream;
        node->size = (uint64_t)-1;
    }
    return EOK;
}

static size_t nmfs_read(void *file, void *addr, size_t offset, size_t size)
{
    nmfs_handle_t *handle = (nmfs_handle_t *)file;
    if (handle == NULL || addr == NULL || handle->kind == NMFS_KIND_ROOT) {
        return VFS_STATUS_FAILED;
    }

    char text[512];
    size_t text_len = 0;
    memset(text, 0, sizeof(text));

    if (handle->kind == NMFS_KIND_STATUS) {
        text_len = nmfs_build_status_text(text, sizeof(text));
    } else if (handle->kind == NMFS_KIND_STATE) {
        text_len = nmfs_build_state_text(text, sizeof(text));
    }

    if (text_len >= sizeof(text)) {
        text_len = sizeof(text) - 1;
    }
    return nmfs_copy_text_out(text, text_len, addr, offset, size);
}

static int nmfs_dummy()
{
    return -ENOSYS;
}

static struct vfs_callback nmfs_callbacks = {
    .mount    = nmfs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = nmfs_open,
    .close    = (vfs_close_t)empty,
    .read     = nmfs_read,
    .write    = (vfs_write_t)nmfs_dummy,
    .readlink = (vfs_readlink_t)nmfs_dummy,
    .mkdir    = (vfs_mk_t)empty,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)nmfs_dummy,
    .symlink  = (vfs_mk_t)nmfs_dummy,
    .stat     = nmfs_stat,
    .ioctl    = (vfs_ioctl_t)nmfs_dummy,
    .dup      = (vfs_dup_t)empty,
    .poll     = (vfs_poll_t)empty,
    .map      = (vfs_mapfile_t)empty,
    .del      = (vfs_del_t)empty,
    .rename   = (vfs_rename_t)empty,
};

int nmfs_setup(void)
{
    nmfs_id = vfs_regist("nmfs", &nmfs_callbacks, NM_REGISTER_ID, 0x4e4d4653);

    vfs_mkdir("/run");
    vfs_mkdir("/run/NetworkManager");
    vfs_node_t nm_root = vfs_open("/run/NetworkManager");
    if (nm_root == NULL) {
        return -ENOENT;
    }
    if (vfs_mount((const char *)NM_REGISTER_ID, nm_root) == VFS_STATUS_FAILED) {
        return -EIO;
    }
    return 0;
}
