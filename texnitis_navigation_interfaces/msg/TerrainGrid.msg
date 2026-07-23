# Terrain layers aligned to a common MapMetaData geometry.
# elevation_m is metres. gradient_x/y are dz/dx and dz/dy (rise/run).
# Empty arrays mean that layer is unavailable. NaN means an unobserved cell.
std_msgs/Header header
nav_msgs/MapMetaData info
float32[] elevation_m
float32[] gradient_x
float32[] gradient_y
uint8[] valid_mask
