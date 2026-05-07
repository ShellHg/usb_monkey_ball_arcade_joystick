// teensy4_usb_descriptor.c
// Override Teensy 4.x USB product/manufacturer strings for any USB type.

#include <avr/pgmspace.h>
#include <usb_names.h>

#define MANUFACTURER_NAME     {'S','h','e','l','l','H','g'}
#define MANUFACTURER_NAME_LEN 7

// Product Name shown to the OS
#define PRODUCT_NAME     {'M','o','n','k','e','y','_','B','a','l','l','_','J','o','y','s','t','i','c','k','_','v','2','.','6'}
#define PRODUCT_NAME_LEN 25

// Override the defaults in the Teensy core to inject the Manufacturer Name and Product Name
PROGMEM struct usb_string_descriptor_struct usb_string_manufacturer_name = {
  2 + MANUFACTURER_NAME_LEN * 2, 3, MANUFACTURER_NAME
};
PROGMEM struct usb_string_descriptor_struct usb_string_product_name = {
  2 + PRODUCT_NAME_LEN * 2, 3, PRODUCT_NAME
};