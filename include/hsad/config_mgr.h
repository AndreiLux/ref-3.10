#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H


/*
 * Function:        is_hw_config_exist
 * Description:     Judge whether a property is exist according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 * Output£º         bool *pvalue                pointer to return value. true for exist, false for not.
 * Return£º         0 for success, error code for failure.
 */
int is_hw_config_exist(const char *node_name, const char *prop_name, bool *pvalue);

/*
 * Function:        get_hw_config_string
 * Description:     Find and read a string from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 *                  int count                   buffer len to achieve the property value, exclude the '\0'.
 * Output£º         char* out_string            pointer to null terminated return string, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_string(const char *node_name, const char *prop_name, char *out_string, int count);

/*
 * Function:        get_hw_config_string_index
 * Description:     Find and read a string from a multiple strings property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 *                  int count                   buffer len to achieve the property value, exclude the '\0'.
 *                  int index                   index of the string in the list of strings.
 * Output£º         char *out_string            pointer to null terminated return string, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_string_index(const char *node_name, const char *prop_name, char *out_string, int count, int index);

/*
 * Function:        get_hw_config_u8
 * Description:     Find and read a 8 bit integer from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 * Output£º         u8 *pvalue                  pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u8(const char *node_name, const char *prop_name, u8 *pvalue);

/*
 * Function:        get_hw_config_u8_array
 * Description:     Find and read an array of 16 bit integers from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 *                  size_t sz                   number of array elements to read.
 * Output£º         u8 *pvalue                  pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u8_array(const char *node_name, const char *prop_name, u8 *pvalue, size_t sz);

/*
 * Function:        get_hw_config_u16
 * Description:     Find and read a 16 bit integer from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 * Output£º         u16 *pvalue                 pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u16(const char *node_name, const char *prop_name, u16 *pvalue);

/*
 * Function:        get_hw_config_u16_array
 * Description:     Find and read an array of 16 bit integers from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
                    size_t sz                   number of array elements to read.
 * Output£º         u16 *pvalue                 pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u16_array(const char *node_name, const char *prop_name, u16 *pvalue, size_t sz);

/*
 * Function:        get_hw_config_u32
 * Description:     Find and read a 32 bit integer from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 * Output£º         u32 *pvalue                 pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u32(const char *node_name, const char *prop_name, u32 *pvalue);

/*
 * Function:        get_hw_config_u32_array
 * Description:     Find and read an array of 32 bit integers from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 *                  size_t sz                   number of array elements to read.
 * Output£º         u32 *pvalue                 pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u32_array(const char *node_name, const char *prop_name, u32 *pvalue, size_t sz);

/*
 * Function:        get_hw_config_u64
 * Description:     Find and read a 64 bit integer from a property according to node name and property name User Input.
 * Data Accessed£º  None
 * Data Updated£º   None
 * Input£º          const char *node_name       name of the device node to be searched.
 *                  const char *prop_name       name of the property to be searched.
 * Output£º         u64 *pvalue                 pointer to return value, modified only if return value is 0.
 * Return£º         0 for success, error code for failure.
 */
int get_hw_config_u64(const char *node_name, const char *prop_name, u64 *pvalue);

#endif
