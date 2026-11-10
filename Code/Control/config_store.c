#include "config_store.h"

#include "stm32f4xx_flash.h"

#include <stddef.h>
#include <string.h>

#define CONTROL_CONFIG_MAGIC       0x52424331UL
#define CONTROL_CONFIG_VERSION     1U
#define CONTROL_CONFIG_FLASH_SECTOR FLASH_Sector_7
#define CONTROL_CONFIG_CRC_SEED    0xFFFFFFFFUL

typedef struct
{
    uint8_t valid;
    uint8_t device;
    uint8_t channel;
    uint8_t reverse;
} ControlConfigStore_BindingEntry;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t entry_count;
    uint32_t payload_crc;
    uint32_t reserved;
    ControlConfigStore_BindingEntry entries[ROBOT_AXIS_COUNT];
} ControlConfigStore_Image;

static uint32_t ControlConfigStore_CalcCrc32(const uint8_t *data, size_t length);
static void ControlConfigStore_BuildImage(const Control_Binding bindings[ROBOT_AXIS_COUNT],
                                          ControlConfigStore_Image *image);
static ErrorStatus ControlConfigStore_ReadImage(Control_Binding bindings[ROBOT_AXIS_COUNT]);
static ErrorStatus ControlConfigStore_WriteImage(const ControlConfigStore_Image *image);
static ErrorStatus ControlConfigStore_VerifyImage(const ControlConfigStore_Image *image);

ErrorStatus ControlConfigStore_LoadBindings(Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    if (bindings == 0)
    {
        return ERROR;
    }

    return ControlConfigStore_ReadImage(bindings);
}

ErrorStatus ControlConfigStore_SaveBindings(const Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    ControlConfigStore_Image image;

    if (bindings == 0)
    {
        return ERROR;
    }

    if (ControlBindingManager_ValidateTable(bindings, 0U, 0) == ERROR)
    {
        return ERROR;
    }

    ControlConfigStore_BuildImage(bindings, &image);
    return ControlConfigStore_WriteImage(&image);
}

static uint32_t ControlConfigStore_CalcCrc32(const uint8_t *data, size_t length)
{
    uint32_t crc;
    size_t index;
    uint8_t bit;

    if (data == 0)
    {
        return 0U;
    }

    crc = CONTROL_CONFIG_CRC_SEED;
    for (index = 0U; index < length; index++)
    {
        crc ^= (uint32_t)data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

static void ControlConfigStore_BuildImage(const Control_Binding bindings[ROBOT_AXIS_COUNT],
                                          ControlConfigStore_Image *image)
{
    Robot_AxisId axis;

    (void)memset(image, 0, sizeof(*image));
    image->magic = CONTROL_CONFIG_MAGIC;
    image->version = CONTROL_CONFIG_VERSION;
    image->entry_count = ROBOT_AXIS_COUNT;

    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        image->entries[axis].valid = (bindings[axis].valid != 0U) ? 1U : 0U;
        image->entries[axis].device = (uint8_t)bindings[axis].device;
        image->entries[axis].channel = bindings[axis].channel;
        image->entries[axis].reverse = (bindings[axis].reverse != 0U) ? 1U : 0U;
    }

    image->payload_crc = ControlConfigStore_CalcCrc32((const uint8_t *)image->entries,
                                                      sizeof(image->entries));
}

static ErrorStatus ControlConfigStore_ReadImage(Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    const ControlConfigStore_Image *image;
    Robot_AxisId axis;

    image = (const ControlConfigStore_Image *)CONTROL_CONFIG_STORE_FLASH_ADDRESS;
    if (ControlConfigStore_VerifyImage(image) == ERROR)
    {
        return ERROR;
    }

    (void)memset(bindings, 0, sizeof(Control_Binding) * ROBOT_AXIS_COUNT);
    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        bindings[axis].valid = image->entries[axis].valid;
        bindings[axis].device = (MAX22216_DeviceId)image->entries[axis].device;
        bindings[axis].channel = image->entries[axis].channel;
        bindings[axis].reverse = image->entries[axis].reverse;
    }

    return ControlBindingManager_ValidateTable(bindings, 0U, 0);
}

static ErrorStatus ControlConfigStore_WriteImage(const ControlConfigStore_Image *image)
{
    uint32_t address;
    const uint32_t *word;
    size_t word_count;
    size_t index;
    FLASH_Status flash_status;

    if ((image == 0) || (ControlConfigStore_VerifyImage(image) == ERROR))
    {
        return ERROR;
    }

    word = (const uint32_t *)image;
    word_count = (sizeof(*image) + sizeof(uint32_t) - 1U) / sizeof(uint32_t);
    address = CONTROL_CONFIG_STORE_FLASH_ADDRESS;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    flash_status = FLASH_EraseSector(CONTROL_CONFIG_FLASH_SECTOR, VoltageRange_3);
    if (flash_status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return ERROR;
    }

    for (index = 0U; index < word_count; index++)
    {
        flash_status = FLASH_ProgramWord(address, word[index]);
        if (flash_status != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return ERROR;
        }
        address += sizeof(uint32_t);
    }

    FLASH_Lock();

    if (memcmp((const void *)CONTROL_CONFIG_STORE_FLASH_ADDRESS,
               image,
               sizeof(*image)) != 0)
    {
        return ERROR;
    }

    return SUCCESS;
}

static ErrorStatus ControlConfigStore_VerifyImage(const ControlConfigStore_Image *image)
{
    uint32_t crc;

    if ((image == 0) ||
        (image->magic != CONTROL_CONFIG_MAGIC) ||
        (image->version != CONTROL_CONFIG_VERSION) ||
        (image->entry_count != ROBOT_AXIS_COUNT))
    {
        return ERROR;
    }

    crc = ControlConfigStore_CalcCrc32((const uint8_t *)image->entries,
                                       sizeof(image->entries));
    return (crc == image->payload_crc) ? SUCCESS : ERROR;
}
