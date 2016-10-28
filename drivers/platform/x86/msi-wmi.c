/*
 * MSI WMI hotkeys
 *
 * Copyright (C) 2009 Novell <trenn@suse.de>
 *
 * Most stuff taken over from hp-wmi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/acpi.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <acpi/video.h>

MODULE_AUTHOR("Thomas Renninger <trenn@suse.de>");
MODULE_DESCRIPTION("MSI laptop WMI hotkeys driver");
MODULE_LICENSE("GPL");

#define DRV_NAME "msi-wmi"

// 00000000-0000-0000-0000-448A5BF04FE4 - power switch mode
#define MSIWMI_BIOS_GUID "551A1F84-FBDD-4125-91DB-3EA8F44F1D45"
#define MSIWMI_MSI_EVENT_GUID "B6F3EEF2-3D2F-49DC-9DE3-85BCE18C62F2"
#define MSIWMI_WIND_EVENT_GUID "5B3CC38A-40D9-7245-8AE6-1145B751BE3F"

MODULE_ALIAS("wmi:" MSIWMI_BIOS_GUID);
MODULE_ALIAS("wmi:" MSIWMI_MSI_EVENT_GUID);
MODULE_ALIAS("wmi:" MSIWMI_WIND_EVENT_GUID);

enum msi_scancodes {
	/* Generic MSI keys (not present on MSI Wind) */
	MSI_KEY_BRIGHTNESSUP	= 0xD0,
	MSI_KEY_BRIGHTNESSDOWN,
	MSI_KEY_VOLUMEUP,
	MSI_KEY_VOLUMEDOWN,
	MSI_KEY_MUTE,
	/* MSI Wind keys */
	WIND_KEY_TOUCHPAD	= 0x08,	 /* Fn+F3 touchpad toggle */
	WIND_KEY_BLUETOOTH	= 0x56,	 /* Fn+F11 Bluetooth toggle */
	WIND_KEY_CAMERA,		 /* Fn+F6 webcam toggle */
	WIND_KEY_WLAN		= 0x5f,	 /* Fn+F11 Wi-Fi toggle */ /* recieved from airplane mode */
	WIND_KEY_TURBO,			 /* Fn+F10 turbo mode toggle */
	WIND_KEY_ECO		= 0x69,	 /* Fn+F10 ECO mode toggle */
	/* MSI GP70 keys */
	GP_KEY_EJECTCD          = 0x0A,  /* Eject CD */
	GP_KEY_DISPLAY_OFF      = 0x59,  /* KEY_DISPLAY_OFF or KEY_SCREENLOCK or KEY_SCREEN or KEY_SCREENSAVER */
	/* GP_KEY_AIRPLANE         = ???, */ /* Fn+F10 and HwKey KEY_WLAN and KEY_BLUETOOTH */
	GP_KEY_COOLERBOOST      = 0x04,  /* atkbd: code 0xbd (Use 'setkeycodes e03d <keycode>' to make it known) */ 	
	GP_KEY_WLAN_ON          = 0x25F, /* Switch wlan to on */
	GP_KEY_WWLAN_ON         = 0x15F, /* Switch wlan to on */
	GP_KEY_BLUETOOTH_ON     = 0xA5F, /* Switch bluetooth to on */
	GP_KEY_WLAN_OFF         = 0xB5F, /* Switch wlan to off */
	GP_KEY_WWLAN_OFF        = 0x95F, /* Switch wlan to off - recived if bt and wifi is on*/
	GP_KEY_WIFI_OFF         = 0x85F, /* Switch wifi to off - received after 0x5f (WIND_KEY_WLAN) */
	GP_KEY_BLUETOOTH_OFF    = 0x35F, /* Switch bluetooth to off */
	GP_KEY_GPANEL           = 0x29,  /* Game Panel or User Defined - KEY_CONTROLPANEL*/
	GP_KEY_KEY_TOUCHPAD_ON  = 0x208, /* Fn+F3 and HwKey */
	/* GP_KEY_KEY_TOUCHPAD_OFF = 0x08, */ /* Fn+F3 and HwKey */
	/* GP_KEY_SWITCHVIDEOMODE  = ???, */ /* Fn+F2 Switch Video Output (handled by ACPI) */
	GP_KEY_P1               = 0x6F,  /* Fn+F4: Dragon Gaming or User Defined */
	GP_KEY_ECO              = 0x279, /* Fn+F5 ECO Engine or User Defined */
	GP_KEY_CAMERA_ON        = 0xA57, /* KEY_CAMERA */
	GP_KEY_CAMERA_OFF       = 0x857, /* KEY_CAMERA */
	GP_KEY_SUSPEND          = 0x3F,  /* Fn+F12 Suspend */
	GP_KEY_BRIGHTNESSUP     = 0x463, /* Fn+ArrowUp */
	GP_KEY_BRIGHTNESSDOWN   = 0x462, /* Fn+ArrowDown */
	GP_KEY_VOLUMEUP         = 0x32,  /* Fn+ArrowRight */
	GP_KEY_VOLUMEDOWN       = 0x21,  /* Fn+ArrowLeft */
	GP_KEY_MUTE             = 0x23,  /* Fn+KP0 */
};

static struct key_entry msi_wmi_keymap[] = {
	{ KE_KEY, MSI_KEY_BRIGHTNESSUP,		{KEY_BRIGHTNESSUP} },
	{ KE_KEY, MSI_KEY_BRIGHTNESSDOWN,	{KEY_BRIGHTNESSDOWN} },
	{ KE_KEY, MSI_KEY_VOLUMEUP,		{KEY_VOLUMEUP} },
	{ KE_KEY, MSI_KEY_VOLUMEDOWN,		{KEY_VOLUMEDOWN} },
	{ KE_KEY, MSI_KEY_MUTE,			{KEY_MUTE} },

	/* These keys work without WMI. Ignore them to avoid double keycodes */
	{ KE_IGNORE, WIND_KEY_TOUCHPAD,		{KEY_TOUCHPAD_TOGGLE} },
	{ KE_IGNORE, WIND_KEY_BLUETOOTH,	{KEY_BLUETOOTH} },
	{ KE_IGNORE, WIND_KEY_CAMERA,		{KEY_CAMERA} },
	{ KE_KEY, WIND_KEY_WLAN,		{KEY_WLAN} },

	/* These are unknown WMI events found on MSI Wind */
	{ KE_IGNORE, 0x00 },
	{ KE_IGNORE, 0x62 },
	{ KE_IGNORE, 0x63 },

	/* These are MSI Wind keys that should be handled via WMI */
	{ KE_KEY, WIND_KEY_TURBO,		{KEY_PROG1} },
	{ KE_KEY, WIND_KEY_ECO,			{KEY_PROG2} },

	/* MSI GP70 keys */
	{ KE_IGNORE, GP_KEY_EJECTCD,		{ KEY_EJECTCD } },
	{ KE_KEY, GP_KEY_DISPLAY_OFF,		{ KEY_SCREENLOCK } },
	/* GP_KEY_AIRPLANE         = ???, atkbd: code 0xbd (Use 'setkeycodes e03d <keycode>' to make it known) */
	{ KE_IGNORE, GP_KEY_COOLERBOOST },
	/* Doing circle: WLAN On -> BT On -> WLAN Off -> BT Off */
	{ KE_KEY, GP_KEY_WLAN_ON,		{ KEY_WLAN } },
	{ KE_KEY, GP_KEY_WWLAN_ON,		{ KEY_WLAN } },
	{ KE_KEY, GP_KEY_BLUETOOTH_ON,		{ KEY_BLUETOOTH } },
	{ KE_KEY, GP_KEY_WLAN_OFF,		{ KEY_WLAN } },
	{ KE_KEY, GP_KEY_BLUETOOTH_OFF,		{ KEY_BLUETOOTH } },
	{ KE_KEY, GP_KEY_WWLAN_OFF,		{ KEY_BLUETOOTH } },
	{ KE_KEY, GP_KEY_WIFI_OFF,		{ KEY_WLAN } },
	{ KE_KEY, GP_KEY_GPANEL,		{ KEY_PROG1 } },
	{ KE_IGNORE, GP_KEY_KEY_TOUCHPAD_ON,	{ KEY_TOUCHPAD_ON } },
	/* { KE_IGNORE, GP_KEY_TOUCHPAD_OFF,	{ KEY_TOUCHPAD_OFF } }, */
	/* GP_KEY_SWITCHVIDEOMODE  = ???, Fn+F2 Switch Video Output (handled by ACPI) */
	{ KE_KEY, GP_KEY_P1,			{ KEY_PROG2 } },
	{ KE_KEY, GP_KEY_ECO,			{ KEY_PROG3 } },
	{ KE_IGNORE, GP_KEY_CAMERA_ON,		{ KEY_CAMERA } },
	{ KE_IGNORE, GP_KEY_CAMERA_OFF,		{ KEY_CAMERA } },
	{ KE_KEY, GP_KEY_SUSPEND,		{ KEY_SUSPEND } },
	{ KE_IGNORE, GP_KEY_BRIGHTNESSUP,	{ KEY_BRIGHTNESSUP } },
	{ KE_IGNORE, GP_KEY_BRIGHTNESSDOWN,	{ KEY_BRIGHTNESSDOWN } },
	{ KE_IGNORE, GP_KEY_VOLUMEUP,		{ KEY_VOLUMEUP } },
	{ KE_IGNORE, GP_KEY_VOLUMEDOWN,		{ KEY_VOLUMEDOWN } },
	{ KE_IGNORE, GP_KEY_MUTE,		{ KEY_MUTE } },
	{ KE_END, 0 }
};

static ktime_t last_pressed;

static const struct {
	const char *guid;
	bool quirk_last_pressed;
} *event_wmi, event_wmis[] = {
	{ MSIWMI_MSI_EVENT_GUID, true },
	{ MSIWMI_WIND_EVENT_GUID, false },
};

static struct backlight_device *backlight;

static int backlight_map[] = { 0x00, 0x33, 0x66, 0x99, 0xCC, 0xFF };

static struct input_dev *msi_wmi_input_dev;

static int msi_wmi_query_block(int instance, int *ret)
{
	acpi_status status;
	union acpi_object *obj;

	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };

	status = wmi_query_block(MSIWMI_BIOS_GUID, instance, &output);

	obj = output.pointer;
	pr_info("query block returned object "
		"type: %d - buffer length: %d\n", obj->type,
		obj->type == ACPI_TYPE_BUFFER ?
		obj->buffer.length : 0);

	if (!obj || obj->type != ACPI_TYPE_INTEGER) {
		if (obj) {
			pr_err("query block returned object "
			       "type: %d - buffer length:%d\n", obj->type,
			       obj->type == ACPI_TYPE_BUFFER ?
			       obj->buffer.length : 0);
		}
		kfree(obj);
		return -EINVAL;
	}
	*ret = obj->integer.value;
	kfree(obj);
	return 0;
}

static int msi_wmi_set_block(int instance, int value)
{
	acpi_status status;

	struct acpi_buffer input = { sizeof(int), &value };

	pr_debug("Going to set block of instance: %d - value: %d\n",
		 instance, value);

	status = wmi_set_block(MSIWMI_BIOS_GUID, instance, &input);

	return ACPI_SUCCESS(status) ? 0 : 1;
}

static int bl_get(struct backlight_device *bd)
{
	int level, err, ret;

	/* Instance 1 is "get backlight", cmp with DSDT */
	err = msi_wmi_query_block(1, &ret);
	if (err) {
		pr_err("Could not query backlight: %d\n", err);
		return -EINVAL;
	}
	pr_debug("Get: Query block returned: %d\n", ret);
	for (level = 0; level < ARRAY_SIZE(backlight_map); level++) {
		if (backlight_map[level] == ret) {
			pr_debug("Current backlight level: 0x%X - index: %d\n",
				 backlight_map[level], level);
			break;
		}
	}
	if (level == ARRAY_SIZE(backlight_map)) {
		pr_err("get: Invalid brightness value: 0x%X\n", ret);
		return -EINVAL;
	}
	return level;
}

static int bl_set_status(struct backlight_device *bd)
{
	int bright = bd->props.brightness;
	if (bright >= ARRAY_SIZE(backlight_map) || bright < 0)
		return -EINVAL;

	/* Instance 0 is "set backlight" */
	return msi_wmi_set_block(0, backlight_map[bright]);
}

static const struct backlight_ops msi_backlight_ops = {
	.get_brightness	= bl_get,
	.update_status	= bl_set_status,
};

static void msi_wmi_notify(u32 value, void *context)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	static struct key_entry *key;
	union acpi_object *obj;
	acpi_status status;

	status = wmi_get_event_data(value, &response);
	if (status != AE_OK) {
		pr_info("bad event status 0x%x\n", status);
		return;
	}

	obj = (union acpi_object *)response.pointer;

	if (obj) {
		int eventcode = 0;
		if (obj->type == ACPI_TYPE_INTEGER) {
			eventcode = obj->integer.value;
		}
		else if (obj->type == ACPI_TYPE_BUFFER) {
			int i;
			pr_debug("Event ACPI_TYPE_BUFFER received\n");
			pr_debug("Buffer length=%u data:", obj->buffer.length);
			for (i = 0; i < obj->buffer.length; i++) {
				pr_debug("%#02x ", obj->buffer.pointer[i]);
			}
			if (obj->buffer.length == 2) {
				eventcode = (int)obj->buffer.pointer[1] << 8 | obj->buffer.pointer[0];
			}
			else {
				pr_info("Unknown event length: %u\n", obj->buffer.length);
				goto msi_wmi_notify_exit;
			}
		}
		else {
			pr_info("Unknown event received %#02x\n", obj->type);
			goto msi_wmi_notify_exit;
		}

		pr_debug("Eventcode: %#02x\n", eventcode);
		key = sparse_keymap_entry_from_scancode(msi_wmi_input_dev,
							eventcode);
		if (!key) {
			pr_info("Unknown key pressed - %x\n", eventcode);
			goto msi_wmi_notify_exit;
		}

		if (event_wmi->quirk_last_pressed) {
			ktime_t cur = ktime_get_real();
			ktime_t diff = ktime_sub(cur, last_pressed);
			/* Ignore event if any event happened in a 50 ms
			   timeframe -> Key press may result in 10-20 GPEs */
			if (ktime_to_us(diff) < 1000 * 50) {
				pr_debug("Suppressed key event 0x%X - "
					 "Last press was %lld us ago\n",
					 key->code, ktime_to_us(diff));
				goto msi_wmi_notify_exit;
			}
			last_pressed = cur;
		}

		if (key->type == KE_KEY &&
		    /* Brightness is served via acpi video driver */
		    (backlight ||
		     (key->code != MSI_KEY_BRIGHTNESSUP &&
		      key->code != MSI_KEY_BRIGHTNESSDOWN))) {
			pr_debug("Send key: 0x%X - Input layer keycode: %d\n",
				 key->code, key->keycode);
			sparse_keymap_report_entry(msi_wmi_input_dev, key, 1,
						   true);
		}
	}
	else {
		pr_debug("obj\n");
		return;
	}
	
msi_wmi_notify_exit:
	kfree(response.pointer);
}

static int __init msi_wmi_backlight_setup(void)
{
	int err;
	struct backlight_properties props;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = ARRAY_SIZE(backlight_map) - 1;
	backlight = backlight_device_register(DRV_NAME, NULL, NULL,
					      &msi_backlight_ops,
					      &props);
	if (IS_ERR(backlight))
		return PTR_ERR(backlight);

	err = bl_get(NULL);
	if (err < 0) {
		backlight_device_unregister(backlight);
		return err;
	}

	backlight->props.brightness = err;

	return 0;
}

static int __init msi_wmi_input_setup(void)
{
	int err;

	msi_wmi_input_dev = input_allocate_device();
	if (!msi_wmi_input_dev)
		return -ENOMEM;

	msi_wmi_input_dev->name = "MSI WMI hotkeys";
	msi_wmi_input_dev->phys = "wmi/input0";
	msi_wmi_input_dev->id.bustype = BUS_HOST;

	err = sparse_keymap_setup(msi_wmi_input_dev, msi_wmi_keymap, NULL);
	if (err)
		goto err_free_dev;

	err = input_register_device(msi_wmi_input_dev);

	if (err)
		goto err_free_keymap;

	last_pressed = ktime_set(0, 0);

	return 0;

err_free_keymap:
	sparse_keymap_free(msi_wmi_input_dev);
err_free_dev:
	input_free_device(msi_wmi_input_dev);
	return err;
}

static int __init msi_wmi_init(void)
{
	int err;
	int i;

	for (i = 0; i < ARRAY_SIZE(event_wmis); i++) {
		if (!wmi_has_guid(event_wmis[i].guid))
			continue;
		pr_debug("event_wmis[%u].guid: %s\n", i, event_wmis[i].guid); 

		err = msi_wmi_input_setup();
		if (err) {
			pr_err("Unable to setup input device\n");
			return err;
		}

		err = wmi_install_notify_handler(event_wmis[i].guid,
			msi_wmi_notify, NULL);
		if (ACPI_FAILURE(err)) {
			pr_err("Unable to setup WMI notify handler\n");
			goto err_free_input;
		}

		pr_debug("Event handler installed\n");
		event_wmi = &event_wmis[i];
		break;
	}

	if (wmi_has_guid(MSIWMI_BIOS_GUID) &&
	    acpi_video_get_backlight_type() == acpi_backlight_vendor) {
		err = msi_wmi_backlight_setup();
		if (err) {
			pr_err("Unable to setup backlight device\n");
			goto err_uninstall_handler;
		}
		pr_debug("Backlight device created\n");
	}
	else {
		pr_debug("acpi_video_get_backlight_type(): %#0x\n", acpi_video_get_backlight_type());
	}

	if (!event_wmi && !backlight) {
		pr_err("This machine doesn't have neither MSI-hotkeys nor backlight through WMI\n");
		return -ENODEV;
	}

	return 0;

err_uninstall_handler:
	if (event_wmi)
		wmi_remove_notify_handler(event_wmi->guid);
err_free_input:
	if (event_wmi) {
		sparse_keymap_free(msi_wmi_input_dev);
		input_unregister_device(msi_wmi_input_dev);
	}
	return err;
}

static void __exit msi_wmi_exit(void)
{
	if (event_wmi) {
		wmi_remove_notify_handler(event_wmi->guid);
		sparse_keymap_free(msi_wmi_input_dev);
		input_unregister_device(msi_wmi_input_dev);
	}
	backlight_device_unregister(backlight);
}

module_init(msi_wmi_init);
module_exit(msi_wmi_exit);
