Represents a map sector.

### Inherits <type>[MapObject](MapObject.md)</type>  
All properties and functions of <type>[MapObject](MapObject.md)</type> can be used in addition to those below.

## Properties

| Property | Type | Description |
|:---------|:-----|:------------|
<prop class="ro">textureFloor</prop> | <type>string</type> | The floor texture of the sector
<prop class="ro">textureCeiling</prop> | <type>string</type> | The ceiling texture of the sector
<prop class="ro">heightFloor</prop> | <type>number</type> | The height of the sector's floor
<prop class="ro">heightCeiling</prop> | <type>number</type> | The height of the sector's ceiling
<prop class="ro">lightLevel</prop> | <type>number</type> | The light level of the sector
<prop class="ro">special</prop> | <type>number</type> | The sector's special
<prop class="ro">id</prop> | <type>number</type> | The sector's id (tag)
<prop class="ro">connectedSides</prop> | <type>[MapSide](MapSide.md)\[\]</type> | An array of all sides that make up this sector
<prop class="ro">colour</prop> | <type>[Colour](Colour.md)</type> | The light colour of the sector
<prop class="ro">fogColour</prop> | <type>[Colour](Colour.md)</type> | The fog colour of the sector
<prop class="ro">planeFloor</prop> | <type>[Plane](Plane.md)</type> | The floor plane of the sector
<prop class="ro">planeCeiling</prop> | <type>[Plane](Plane.md)</type> | The ceiling plane of the sector

## Constructors

!!! attention "No Constructors"
    This type can not be created directly in scripts.

## Functions

### `containsPoint`

<listhead>Parameters</listhead>

* <type>[Point](Point.md)</type> <arg>position</arg>: Point coordinates in map units

**Returns** <type>boolean</type>

Returns `true` if <arg>position</arg> is inside the sector.
