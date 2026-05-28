-- Get initial test data
local angle_degrees = test_data["angle_degrees"]
local energy_kilojoules = test_data["energy_kilojoules"]
local mass_kilograms = test_data["mass_kilograms"]
local volume_liters = test_data["volume_liters"]
local temperature_celsius = test_data["temperature_celsius"]
local temperature_fahrenheit = test_data["temperature_fahrenheit"]

-- Set test results
test_data["angle_arcmins"] = Angle.from_degrees(angle_degrees):to_arcmin()
test_data["energy_joules"] = Energy.from_kilojoule(energy_kilojoules):to_joule()
test_data["mass_grams"] = Mass.from_kilogram(mass_kilograms):to_gram()
test_data["volume_milliliters"] = Volume.from_liter(volume_liters):to_milliliter()
test_data["temperature_fahrenheit_from_celsius"] = Temperature.from_celsius(temperature_celsius):to_fahrenheit()
test_data["temperature_celsius_from_fahrenheit"] = Temperature.from_fahrenheit(temperature_fahrenheit):to_celsius()
test_data["temperature_kelvin_from_celsius"] = Temperature.from_celsius(temperature_celsius):to_kelvin()
test_data["temperature_less_than"] = Temperature.from_celsius(temperature_celsius) < Temperature.from_fahrenheit(temperature_fahrenheit)
test_data["temperature_equal_to"] = Temperature.from_celsius(0) == Temperature.from_fahrenheit(32)
