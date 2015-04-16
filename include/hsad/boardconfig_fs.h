#ifndef BOARDCONFIG_FS_H
#define BOARDCONFIG_FS_H

#define NAME_LEN (128)
#define MAX_BUFFER_LEN 1024

struct hw_config_pairs {
    char node_name[NAME_LEN];
    char prop_name[NAME_LEN];
    union {
        bool out_bvalue;             //value to save bool type property

        struct {
            char *p_svalue;          //value to save string type property
            int length;
        }out_string;

        struct {
            int index;
            char *p_svalue;
            int length;
        }out_string_idx;            //value to save one of string list property

        u8 out_u8value;              //value to save u8 type property

        struct {
            size_t sz;
            u8 *p_u8value;
        }out_u8_array;              //value to save u8 array type property

        u16 out_u16value;            //value to save u16 type property

        struct {
            size_t sz;
            u16 *p_u16value;
        }out_u16_array;             //value to save u16 array type property

        u32 out_u32value;            //value to save u32 type property

        struct {
            size_t sz;
            u32 *p_u32value;
        }out_u32_array;             //value to save u32 array type property

        u64 out_u64value;            //value to save u64 type property
    };
};

#define MODULE_IOCTL_MAGIC      'x'

#define CMD_HW_BOOL_GET             _IOR(MODULE_IOCTL_MAGIC, 1, struct hw_config_pairs)
#define CMD_HW_STRING_GET           _IOR(MODULE_IOCTL_MAGIC, 2, struct hw_config_pairs)
#define CMD_HW_STRING_IDX_GET       _IOR(MODULE_IOCTL_MAGIC, 3, struct hw_config_pairs)
#define CMD_HW_U8_GET               _IOR(MODULE_IOCTL_MAGIC, 4, struct hw_config_pairs)
#define CMD_HW_U8_ARRAY_GET         _IOR(MODULE_IOCTL_MAGIC, 5, struct hw_config_pairs)
#define CMD_HW_U16_GET              _IOR(MODULE_IOCTL_MAGIC, 6, struct hw_config_pairs)
#define CMD_HW_U16_ARRAY_GET        _IOR(MODULE_IOCTL_MAGIC, 7, struct hw_config_pairs)
#define CMD_HW_U32_GET              _IOR(MODULE_IOCTL_MAGIC, 8, struct hw_config_pairs)
#define CMD_HW_U32_ARRAY_GET        _IOR(MODULE_IOCTL_MAGIC, 9, struct hw_config_pairs)
#define CMD_HW_U64_GET              _IOR(MODULE_IOCTL_MAGIC, 10, struct hw_config_pairs)

#endif
