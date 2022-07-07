# remove vconsole support
PACKAGECONFIG:remove = "vconsole"

# remove locale and internationalization
PACKAGECONFIG:remove = "localed"

# other stuff we don't use
PACKAGECONFIG:remove = "rfkill"
PACKAGECONFIG:remove = "hibernate"
