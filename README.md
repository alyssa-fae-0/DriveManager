# DriveManager
This utility will automatically move files between a primary SSD and backup data HDD without the user having to worry about manually freeing up room on their SSD.

Features will likely include:
  Automatic file/folder relocation with necissary symlinking
  Ability to mark specific files/folders as static (meaning they won't be relocated)
    Example: the Windows folder probably shouldn't be relocated to a different drive (though hopefully Windows wouldn't let you move that folder while it was running anyways. Not planning on count on that though)
  Ability to set a minimum amount of free space to keep on the SSD at all times
    When the amount of free space falls below the specified limit, relocation will trigger automatically using the established rules / preferences
	
Features to consider (these are vague/experimental ideas)
  Compression of files/folders that are relocated to the data HDD
  Ability to mirror the SSD onto the HDD
    Combining the two above, compress the mirrored data (seems unlikely to be technologically feasible, but I'll try it anyways)
