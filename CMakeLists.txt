# Create the extension library
ADD_LIBRARY(Extensions_AssimpResource
  Resources/AssimpResource.h
  Resources/AssimpResource.cpp
)

TARGET_LINK_LIBRARIES(Extensions_AssimpResource
  OpenEngine_Resources
  # Extension dependencies
  ${ASSIMP_LIBRARIES}
)
