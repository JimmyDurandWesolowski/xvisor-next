#ifndef _LINUX_ACPI_H
#define _LINUX_ACPI_H

/* Note: This file is just a place holder */

#ifndef CONFIG_ACPI
#define ACPI_COMPANION(dev)		(NULL)
#define ACPI_COMPANION_SET(dev, adev)	do {} while (0)

struct acpi_device;

static inline const char *acpi_dev_name(struct acpi_device *adev)
{
	return NULL;
}

static inline bool acpi_driver_match_device(struct device *dev,
                                            const struct device_driver *drv)
{
	return false;
}
#endif /* CONFIG_ACPI */

#endif
