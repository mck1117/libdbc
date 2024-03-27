# libdbc: CAN DBC Format Parser and Decoder

 - Parses DBC files in to messages & signals
 - Parses raw CAN frames in to "human values" based on the parsed DBC

## `dbcconvert` Tool

Reads a DBC and raw CAN dump, and converts to a csv of converted values.

#### Usage

tl;dr `./dbcconvert -dbc <mydbc.dbc> -in <my_dump.csv> -out <my_output.csv> [-sparse]`

The `-sparse` option will skip values when they haven't changed since the previous row. Reading this format works in MegaLogViewer, but may not be supported by other log viewer software. This saves considerable output size, as one row is emitted per input CAN frame. Lots of different frames means that each frame only changes a few values.
