#include <linux/of.h>
#include <hsad/config_mgr.h>
#include <linux/hw_log.h>

#undef HWLOG_TAG
#define HWLOG_TAG config_mgr
HWLOG_REGIST();

#define HW_PUBLIC   "hw,public"

static int is_property_public(struct device_node *np, const char *prop_name)
{
    int ret;
    int len;
    struct property *pp;

    pp = of_find_property(np, HW_PUBLIC, NULL);
    if(!pp) {
        hwlog_err("of_find_property failed\n");
        return 0;
    }

    len = pp->length;
    if(len == 0) {
        return 1;
    }

    ret = of_property_match_string(np, HW_PUBLIC, prop_name);
    if(ret < 0) {
        hwlog_err("of_property_match_string error, ret = %d\n", ret);
        return 0;
    }

    return 1;
}

int is_hw_config_exist(const char *node_name,
        const char *prop_name, bool *pvalue)
{
    struct device_node *np;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    *pvalue = of_property_read_bool(np, prop_name);

    of_node_put(np);

    return 0;
}

int get_hw_config_string(const char *node_name,
        const char *prop_name, char *out_string,
        int count)
{
    struct device_node *np;
    const char *out_value;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        out_string == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_string(np, prop_name, &out_value);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

    strncpy(out_string, out_value, count);

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_string_index(const char *node_name,
        const char *prop_name, char *out_string,
        int count, int index)
{
    struct device_node *np;
    const char *out_value;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        out_string == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_string_index(np, prop_name, index, &out_value);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

    strncpy(out_string, out_value, count);

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u8(const char *node_name,
        const char *prop_name, u8 *pvalue)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u8(np, prop_name, pvalue);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u8_array(const char *node_name,
        const char *prop_name, u8 *pvalue,
        size_t sz)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u8_array(np, prop_name, pvalue, sz);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u16(const char *node_name,
        const char *prop_name, u16 *pvalue)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u16(np, prop_name, pvalue);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u16_array(const char *node_name,
        const char *prop_name, u16 *pvalue,
        size_t sz)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u16_array(np, prop_name, pvalue, sz);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u32(const char *node_name,
        const char *prop_name, u32 *pvalue)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u32(np, prop_name, pvalue);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u32_array(const char *node_name,
        const char *prop_name, u32 *pvalue,
        size_t sz)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u32_array(np, prop_name, pvalue, sz);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}

int get_hw_config_u64(const char *node_name,
        const char *prop_name, u64 *pvalue)
{
    struct device_node *np;
    int ret;

    if(node_name == NULL ||
        prop_name == NULL ||
        pvalue == NULL) {
        hwlog_err("Invalid Argument, NULL passed\n");
        return -EINVAL;
    }

    np = of_find_node_by_name(NULL, node_name);
    if (!np) {
        hwlog_err("can not get device node with node_name: %s\n", node_name);
        return -ENODEV;
    }

    if(0 == is_property_public(np, prop_name)) {
        hwlog_err("property to read is not public, permission denied\n");
        ret = -EACCES;
        goto OUT;
    }

    ret = of_property_read_u64(np, prop_name, pvalue);
    if (ret != 0) {
        hwlog_err("can not get prop values with prop_name: %s\n", prop_name);
        goto OUT;
    }

OUT:
    of_node_put(np);

    return ret;
}
