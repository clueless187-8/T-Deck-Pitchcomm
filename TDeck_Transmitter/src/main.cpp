  touch.setPins(-1, TDECK_TOUCH_INT);
  if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TDECK_I2C_SDA, TDECK_I2C_SCL)) {
    Serial.println("Failed to find GT911 - trying alternate address...");
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_H, TDECK_I2C_SDA, TDECK_I2C_SCL)) {
      Serial.println("GT911 touch init FAILED!");
    } else {
      Serial.println("GT911 touch init SUCCESS (0x28)");
    }
  } else {
    Serial.println("GT911 touch init SUCCESS (0x14)");
  }
