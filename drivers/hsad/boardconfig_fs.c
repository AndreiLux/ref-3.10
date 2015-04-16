#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <hsad/boardconfig_fs.h>
#include <hsad/config_mgr.h>
#include <linux/errno.h>
#include <linux/hw_log.h>

#undef HWLOG_TAG
#define HWLOG_TAG boardconfig_fs
HWLOG_REGIST();

#define HW_BOARDCONFIG_DEV_NAME "boardconfig_fs"
#define PROP_NAME "hw,public"
#define BOARDCONFIG_FILE_SIZE   (PAGE_SIZE * 2)


static int is_string(unsigned char *p, int len)
{
    int i;

    for (i = 0; i < len; i++) {
    unsigned char val = p[i];

    if ((i && !val) ||
        (val >= ' ' && val <= '~'))
        continue;

        return 0;
    }

    return 1;
}

/**
 * format property value read from device node.
 * if is a string or string list: show it as:
 *     |--- 20 bytes ---|--- 20 bytes ---|--- stringA + stringB + stringC ---|
 *     node_name         property_name    string values
 *
 * if is a value or value list with length <= 3, show it as in 1 bytes:
 *     |--- 20 bytes ---|--- 20 bytes ---|--- %02x.%02x.%02x ---|
       node_name         property_name    1bytes values
 *
 * or is a value or value list with length > 3, show it as in 4 bytes:
 *     |--- 20 bytes ---|--- 20 bytes ---|--- %08x.%08x.%08x ---|
 *     node_name         property_name    4bytes values
 */
static int format_property(char **ptr, struct device_node *np,
        struct property *prop, int *cnt)
{
    int proplen;
    void *pval;
    int len;
    int ret = 0;
    char *prev_ptr = *ptr;

    /*show the node name and the property name*/
    len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "%-20s%-20s",
            np->name, prop->name);
    *ptr += len;
    *cnt += len;
    if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
        ret = -1;
        goto OUT;
    }

    proplen = prop->length;
    pval = prop->value;

    /**
     * if the property value is a string or string list,
     * show it as is "string + string + string"
     */
    if (is_string(pval, proplen)) {
        while (proplen > 0) {
            int n = strlen(pval);

            len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "%s",
                    (char*)pval);
            *ptr += len;
            *cnt += len;

            /*reach to the max buffer*/
            if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
                ret = -1;
                goto OUT;
            }

            /* Skip over the NULL byte too.  */
            pval += n + 1;
            proplen -= n + 1;

            if (proplen > 0) {
                len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "%s",
                        " + ");
                *ptr += len;
                *cnt += len;

                /*reach to the max buffer*/
                if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
                    ret = -1;
                    goto OUT;
                }
            }
        }
        len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "\n");
        *ptr += len;
        *cnt += len;

        /*reach to the max buffer*/
        if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
            ret = -1;
            goto OUT;
        }
    } else {
        /**
         * property is not a string,
         * and length is <= 3 bytes,
         * show it as is "0x%02x.0x%02x.0x%02x"
         */
        if (proplen & 3) {
            while (proplen) {
                proplen--;
                if (proplen)
                    len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "0x%02x.",
                            *(unsigned char *) pval);
                else
                    len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "0x%02x",
                            *(unsigned char *) pval);

                *ptr += len;
                *cnt += len;
                pval++;

                /*reach to the max buffer*/
                if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
                    ret = -1;
                    goto OUT;
                }
            }
        } else {
            /**
             * property is not a string,
             * and length is > 3 bytes,
             * show it as is "0x%08x.0x%08x.0x%08x"
             */
            while (proplen >= 4) {
                proplen -= 4;

                if (proplen)
                    len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "0x%08x.",
                            be32_to_cpup((unsigned int *) pval));
                else
                    len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "0x%08x",
                            be32_to_cpup((unsigned int *) pval));

                *ptr += len;
                *cnt += len;
                pval += 4;

                /*reach to the max buffer*/
                if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
                    ret = -1;
                    goto OUT;
                }
            }
        }
        len = scnprintf(*ptr, BOARDCONFIG_FILE_SIZE - *cnt, "\n");
        *ptr += len;
        *cnt += len;

        /*reach to the max buffer*/
        if(*cnt == BOARDCONFIG_FILE_SIZE - 1) {
            ret = -1;
            goto OUT;
        }
    }

OUT:
    if(ret == -1) {
        *prev_ptr = '\0';
    }
    return ret;
}

/**
 * travelsal all the device node, find out the node with property: hw,public
 * count the strings of property,
 * if the property has no value, or the count is 0,
 * show all the properties in the device node.
 * or show the property values display in: hw, public.
 *
 * Example:
 * node1 {
 *     str_prop = "a string";
 *     u32_prop = <0>;
 *     hw,public;
 * };
 *
 * node2 {
 *     str_prop = "a string";
 *     u32_prop = <0>;
 *     hw,public = "str_prop";
 * };
 *
 * In node1, hw,public has no value, so all the property will be shown:
 *  node1               str_prop            a string
 *  node1               u32_prop            0x00000000
 *  node1               hw,public
 *
 * In node2, hw,public indicate only str_prop is public, shown as:
 *  node2               str_prop            a string
 */
static ssize_t hw_boardconfig_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    char *ptr, *array_tmp_ptr;
    struct device_node *np;
    int i, count;
    const char *prop_name;
    struct property *prop;
    struct property *pp;
    int len = 0, cnt = 0;
    int ret;

    ptr = (char*) kzalloc((BOARDCONFIG_FILE_SIZE * sizeof(char)), GFP_KERNEL);
    if (!ptr) {
        hwlog_err("%s: memory alloc failed for public property\n",__func__);
        return -ENOMEM;
    }

    array_tmp_ptr = ptr;

    for_each_node_with_property(np, PROP_NAME) {
        /**
         * find the property: hw,public, and calculate the length
         * of the property, if the length is 0, marks as the property
         * has no value, show all the property in the node.
         */
        pp = of_find_property(np, PROP_NAME, NULL);
        if(pp == NULL) {
            hwlog_warn("error to get property: hw,public, skip...\n");
            continue;
        }
        if (pp->length == 0) {
            for_each_property_of_node(np, prop) {
                ret = format_property(&ptr, np, prop, &cnt);
                if(ret == -1) {
                    hwlog_warn("no enough buffer for display all the public property\n");
                    goto OUT;
                }
            }
        } else {
            /**
             * only show property value specified in the property: "hw,public"
             * circulate the string list, get the property name, and read the proprety value.
             * if the property is not exist, skip it...
             */
            count = of_property_count_strings(np, PROP_NAME);
            for(i = 0; i < count; i++) {
                ret = of_property_read_string_index(np, PROP_NAME, i, &prop_name);
                if (ret != 0) {
                    hwlog_warn("error to get property name, ret = %d, skip...\n", ret);
                    continue;
                }
                prop = of_find_property(np, prop_name, NULL);
                if (!prop) {
                    hwlog_warn("can not find property with name: %s, skip...\n", prop_name);
                    continue;
                }

                ret = format_property(&ptr, np, prop, &cnt);
                if(ret == -1) {
                    hwlog_warn("no enough buffer for display all the public property\n");
                    goto OUT;
                }
            }
        }
    }

OUT:
    len = strlen(array_tmp_ptr);

    len = simple_read_from_buffer(buf, size, ppos, (void*) array_tmp_ptr, len);

    kfree(array_tmp_ptr);

    return len;
}

static long hw_boardconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    char *node_name = NULL;
    char *prop_name = NULL;
    void __user *argp = (void __user*)arg;
    struct hw_config_pairs __user *uargs = (struct hw_config_pairs __user *)argp;

    node_name = (char *)kzalloc(sizeof(char) * NAME_LEN, GFP_KERNEL);
    if(node_name == NULL) {
        hwlog_err("%s: memory alloc failed for node_name\n",__func__);
        ret = -ENOMEM;
        goto ERROR_OUT;
    }

    prop_name = (char *)kzalloc(sizeof(char) * NAME_LEN, GFP_KERNEL);
    if(prop_name == NULL) {
        hwlog_err("%s: space alloc failed for prop_name\n",__func__);
        ret = -ENOMEM;
        goto ERROR_OUT;
    }

    /*get node name intend to read from dts*/
    if(0 != copy_from_user(node_name, uargs->node_name, NAME_LEN - 1)) {
        hwlog_err("%s: get node_name failed, copy_from_user EFAULT\n",__func__);
        ret = -EFAULT;
        goto ERROR_OUT;
    }

    /*get property name intend to read from dts*/
    if(0 != copy_from_user(prop_name, uargs->prop_name, NAME_LEN - 1)) {
        hwlog_err("%s: get prop_name failed, copy_from_user EFAULT\n",__func__);
        ret = -EFAULT;
        goto ERROR_OUT;
    }

    switch(cmd) {
        /**
         * command to get a property is exist or not
         * the result will be return via the parameter from userspace: out_bvalue
         */
        case CMD_HW_BOOL_GET:
            {
                bool outvalue;

                /*read property data from dts*/
                ret = is_hw_config_exist(node_name, prop_name, &outvalue);
                if(ret == 0) {
                    ret = copy_to_user(&(uargs->out_bvalue), &outvalue, sizeof(outvalue));
                    if(ret != 0) {
                        ret = -EFAULT;
                        hwlog_err("error to copy data to userspace\n");
                    }
                }
            }
            break;

        /**
         * command to get a property value with type is: string
         * the string pointer and its length is passed from userspace
         * the result will be return via the parameter from userspace: out_string
         */
        case CMD_HW_STRING_GET:
            {
                char *poutstring = NULL;
                u32 len;

                /*get string property length intend to read from dts*/
                ret = copy_from_user(&len, &(uargs->out_string.length), sizeof(len));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < len){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                poutstring = (char*)kzalloc((len + 1) * sizeof(char), GFP_KERNEL);
                if(poutstring == NULL) {
                    ret = -ENOMEM;
                    hwlog_err("error to alloc memory\n");
                    break;
                }

                /*read property data from dts*/
                ret = get_hw_config_string(node_name, prop_name, poutstring, len);
                if(ret != 0) {
                    kfree(poutstring);
                    poutstring = NULL;
                    hwlog_err("error to get dts data\n");
                    break;
                }

                ret = copy_to_user(uargs->out_string.p_svalue, poutstring, len * sizeof(char));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to copy data to userspace\n");
                }

                kfree(poutstring);
                poutstring = NULL;
            }
            break;

        /**
         * command to get a string with type is: string list
         * the string pointer, length and the index of the list is passed from userspace
         * the result will be return via the parameter from userspace: out_string_idx
         */
        case CMD_HW_STRING_IDX_GET:
            {
                char *poutstring = NULL;
                u32 index;
                u32 len;

                /*get index of string list property intend to read from dts*/
                ret = copy_from_user(&len, &(uargs->out_string_idx.length), sizeof(len));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < len){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                /*get string property length intend to read from dts*/
                ret = copy_from_user(&index, &(uargs->out_string_idx.index), sizeof(index));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < index){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                poutstring = (char*)kzalloc((len + 1) * sizeof(char), GFP_KERNEL);
                if(poutstring == NULL) {
                    ret = -ENOMEM;
                    hwlog_err("error to alloc memory\n");
                    break;
                }

                /*read property data from dts*/
                ret = get_hw_config_string_index(node_name, prop_name, poutstring, len, index);
                if(ret != 0) {
                    kfree(poutstring);
                    poutstring = NULL;
                    pr_err("error to get dts data\n");
                    break;
                }

                ret = copy_to_user(uargs->out_string_idx.p_svalue, poutstring, len * sizeof(char));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to copy data to userspace\n");
                }

                kfree(poutstring);
                poutstring = NULL;
            }
            break;

        /**
         * command to get a property with type: unsigned char
         * the result will be return via the parameter from userspace: out_u8value
         */
        case CMD_HW_U8_GET:
            {
                u8 outvalue;

                /*read property data from dts*/
                ret = get_hw_config_u8(node_name, prop_name, &outvalue);
                if(ret == 0) {
                    ret = copy_to_user(&(uargs->out_u8value), &outvalue, sizeof(outvalue));
                    if(ret != 0) {
                        ret = -EFAULT;
                        hwlog_err("error to copy data to userspace\n");
                    }
                }
            }
            break;

        /**
         * command to get a property with type: unsigned char array
         * the array pointer and its length is passed from userspace
         * the result will be return via the parameter from userspace: out_u8_array
         */
        case CMD_HW_U8_ARRAY_GET:
            {
                u32 sz;
                u8 *outvalue = NULL;

                /*get array size of u8 array property intend to read from dts*/
                ret = copy_from_user(&sz, &(uargs->out_u8_array.sz), sizeof(sz));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < sz){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                outvalue = (u8*)kzalloc(sz * sizeof(u8), GFP_KERNEL);
                if(outvalue == NULL) {
                    ret = -ENOMEM;
                    hwlog_err("error to alloc memory\n");
                    break;
                }

                /*read property data from dts*/
                ret = get_hw_config_u8_array(node_name, prop_name, outvalue, sz);
                if(ret != 0) {
                    kfree(outvalue);
                    outvalue = NULL;
                    hwlog_err("error to get dts data\n");
                    break;
                }

                ret = copy_to_user(uargs->out_u8_array.p_u8value, outvalue, sz * sizeof(u8));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to copy data to userspace\n");
                }

                kfree(outvalue);
                outvalue = NULL;
            }
            break;

        /**
         * command to get a property with type: unsigned short
         * the result will be return via the parameter from userspace: out_u16value
         */
        case CMD_HW_U16_GET:
            {
                u16 outvalue;

                /*read property data from dts*/
                ret = get_hw_config_u16(node_name, prop_name, &outvalue);
                if(ret == 0) {
                    ret = copy_to_user(&(uargs->out_u16value), &outvalue, sizeof(outvalue));
                    if (ret != 0) {
                        ret = -EFAULT;
                        hwlog_err("error to copy data to userspace\n");
                    }
                }
            }
            break;

        /**
         * command to get a property with type: unsigned short array
         * the array pointer and its length is passed from userspace
         * the result will be return via the parameter from userspace: out_16_array
         */
        case CMD_HW_U16_ARRAY_GET:
            {
                u32 sz;
                u16 *outvalue = NULL;

                /*get array size of u16 array property intend to read from dts*/
                ret = copy_from_user(&sz, &(uargs->out_u16_array.sz), sizeof(sz));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < sz){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                outvalue = (u16*)kzalloc(sz * sizeof(u16), GFP_KERNEL);
                if(outvalue == NULL) {
                    ret = -ENOMEM;
                    hwlog_err("error to alloc memory\n");
                    break;
                }

                /*read property data from dts*/
                ret = get_hw_config_u16_array(node_name, prop_name, outvalue, sz);
                if(ret != 0) {
                    kfree(outvalue);
                    outvalue = NULL;
                    hwlog_err("error to get dts data\n");
                    break;
                }

                ret = copy_to_user(uargs->out_u16_array.p_u16value, outvalue, sz * sizeof(u16));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to copy data to userspace\n");
                }

                kfree(outvalue);
                outvalue = NULL;
            }
            break;

        /**
         * command to get a property with type: unsigned int
         * the result will be return via the parameter from userspace: out_u32value
         */
        case CMD_HW_U32_GET:
            {
                u32 outvalue;

                /*read property data from dts*/
                ret = get_hw_config_u32(node_name, prop_name, &outvalue);
                if(ret == 0) {
                    ret = copy_to_user(&(uargs->out_u32value), &outvalue, sizeof(outvalue));
                    if(ret != 0) {
                        ret = -EFAULT;
                        hwlog_err("error to copy data to userspace\n");
                    }
                }
            }
            break;

        /**
         * command to get a property with type: unsigned int array
         * the array pointer and its length is passed from userspace
         * the result will be return via the parameter from userspace: out_u32_array
         */
        case CMD_HW_U32_ARRAY_GET:
            {
                u32 sz;
                u32 *outvalue = NULL;

                /*get array size of u32 array property intend to read from dts*/
                ret = copy_from_user(&sz, &(uargs->out_u32_array.sz), sizeof(sz));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to get data from userspace\n");
                    break;
                }
                if(MAX_BUFFER_LEN < sz){
                    ret = -EFAULT;
                    hwlog_err("error the len from user is larger than maxsize!\n");
                    break;
                }

                outvalue = (u32*)kzalloc(sz * sizeof(u32), GFP_KERNEL);
                if(outvalue == NULL) {
                    ret = -ENOMEM;
                    hwlog_err("error to alloc memory\n");
                    break;
                }

                /*read property data from dts*/
                ret = get_hw_config_u32_array(node_name, prop_name, outvalue, sz);
                if(ret != 0) {
                    kfree(outvalue);
                    outvalue = NULL;
                    hwlog_err("error to get dts data\n");
                    break;
                }

                ret = copy_to_user(uargs->out_u32_array.p_u32value, outvalue, sz * sizeof(u32));
                if(ret != 0) {
                    ret = -EFAULT;
                    hwlog_err("error to copy data to userspace\n");
                }

                kfree(outvalue);
                outvalue = NULL;
            }
            break;

        /**
         * command to get a property with type: unsigned long long
         * the result will be return via the parameter from userspace: out_u64value
         */
        case CMD_HW_U64_GET:
            {
                u64 outvalue;

                /*read property data from dts*/
                ret = get_hw_config_u64(node_name, prop_name, &outvalue);
                if(ret == 0) {
                    ret = copy_to_user(&(uargs->out_u64value), &outvalue, sizeof(outvalue));
                    if(ret != 0) {
                        ret = -EFAULT;
                        hwlog_err("error to copy data to userspace\n");
                    }
                }
            }
            break;

        default:
            break;
    }

ERROR_OUT:
    if(prop_name != NULL) {
        kfree(prop_name);
        prop_name = NULL;
    }

    if(node_name != NULL) {
        kfree(node_name);
        node_name = NULL;
    }

    return ret;
}

static struct file_operations hw_boardconfig_fops = {
    .owner = THIS_MODULE,
    .read = hw_boardconfig_read,
    .unlocked_ioctl = hw_boardconfig_ioctl,
};

static struct miscdevice hw_boardconfig_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = HW_BOARDCONFIG_DEV_NAME,
    .fops = &hw_boardconfig_fops,
};

static int __init boardconfig_fs_init(void)
{
    return misc_register(&hw_boardconfig_dev);
}

static void __exit boardconfig_fs_exit(void)
{
    return;
}

module_init(boardconfig_fs_init);
module_exit(boardconfig_fs_exit);

MODULE_LICENSE("GPL");
