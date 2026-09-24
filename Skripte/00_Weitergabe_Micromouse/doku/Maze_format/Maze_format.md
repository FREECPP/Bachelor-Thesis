# Maze format

The definition of the format to communicate and save the maze.

![image](C:\Users\robom\Documents\0040_THAB\Semester_4\Informatik_IV\micromouse\doku\Maze_format\Screenshot%202026-04-30%20131227.png)

## The idea

The maze is divided into a grid. The robots can only follow the lines on the maze's floor.

For each grid intersection (node), the state of the walls is stored (0 = wall, 1 = no wall/passage). Since only 4 bits of data are needed for a node, two nodes can be stored in a single byte.

Using the magnetometer, the Earth's magnetic field is used for spatial orientation. The first node is the northwesternmost point of the maze.

## Data struct

The data is stored in the following frame:

| Data             | Description                                              |
| ---------------- | -------------------------------------------------------- |
| uint8_t length_x | Number of nodes in x direction (columns)                 |
| uint8_t length_y | Number of nodes in y direction (rows)                    |
| uint8_t maze[n]  | The maze array with a length of n = ceil(node_count / 2) |

### Bit order

The bit order is defined as follows:

```
uint8_t  xxxx yyyy
         |||| ||||
         NESW NESW
```

`xxxx` describes the first nibble (high bits) with the order North, East, South, West.  
`yyyy` describes the second nibble (low bits) in the same order.

A bit value of `1` means **passage** (no wall), `0` means **wall**.

### Node addressing

Nodes are addressed as `(row, col)`, both zero-indexed, with `(0, 0)` being the northwesternmost node. The x-axis points East (increasing column index), the y-axis points South (increasing row index).

### Node order

Nodes are stored row by row, left to right, top to bottom:

```
Row 0:  (0,0)(0,1)  (0,2)(0,3)  (0,4)(0,5) ...
Row 1:  (1,0)(1,1)  (1,2)(1,3)  (1,4)(1,5) ...
...
```

Each byte holds two consecutive nodes. If the total node count is odd, the low nibble of the last byte is unused and should be set to `0x0`.

**Example** for a 5×5 maze (25 nodes → 13 bytes):

| Byte index | High nibble | Low nibble |
| ---------- | ----------- | ---------- |
| 0          | (0,0)       | (0,1)      |
| 1          | (0,2)       | (0,3)      |
| 2          | (0,4)       | (1,0)      |
| 3          | (1,1)       | (1,2)      |
| ...        | ...         | ...        |
| 12         | (4,4)       | unused     |

### Consistency

Because each wall is stored redundantly — once per adjacent node — inconsistencies can occur when merging data from multiple robots.

**Merge rule:** A wall is considered present if **any** source reports it as a wall (`AND` on the passage bits). This is the conservative choice: prefer blocked over open to avoid collisions.

If inconsistencies are detected (i.e., two adjacent nodes disagree on a shared wall), the robot should flag the affected nodes and trigger a **rescan** of that area before relying on the data for path planning.

```cpp
// Pseudocode: check wall consistency between vertically adjacent nodes
// South wall of (row, col) must match North wall of (row+1, col)
bool checkConsistency(const uint8_t* maze, uint8_t cols, uint8_t rows) {
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols; c++) {
            bool south_of_current = getWall(maze, cols, r,   c, SOUTH);
            bool north_of_below   = getWall(maze, cols, r+1, c, NORTH);
            if (south_of_current != north_of_below) return false; // inconsistency found
        }
    }
    // analogous check for East/West pairs
    return true;
}
```

## Example encoding

Wall bits for node `(0,0)` of the sketch above (outer walls closed, passage to East):

```
N=0  E=1  W=0  S=0  →  0b0100  =  0x4
```
