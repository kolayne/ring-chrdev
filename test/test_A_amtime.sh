function birtht() {
  stat -c %w /dev/ring
}

function accesst() {
  stat -c %x /dev/ring
}

function modifiedt() {
  stat -c %y /dev/ring
}

function test_am_time() {
  birth=$(birtht)
  access=$(accesst)
  modified=$(modifiedt)

  # If did nothing, shall remain
  [ "$birth" = "$(birtht)" ]
  [ "$access" = "$(accesst)" ]
  [ "$modified" = "$(modifiedt)" ]

  # If modified, modified time shall change
  echo -n ab > /dev/ring
  [ "$birth" = "$(birtht)" ]
  [ "$access" = "$(accesst)" ]
  [ "$modified" != "$(modifiedt)" ]
  modified=$(modifiedt)

  # If read, access time shall change
  dd if=/dev/ring bs=1 count=1
  [ "$birth" = "$(birtht)" ]
  [ "$access" != "$(accesst)" ]
  [ "$modified" = "$(modifiedt)" ]
  access=$(accesst)

  # With noatime, shall remain
  dd if=/dev/ring iflag=noatime bs=1 count=1
  [ "$birth" = "$(birtht)" ]
  [ "$access" = "$(accesst)" ]
  [ "$modified" = "$(modifiedt)" ]
}
