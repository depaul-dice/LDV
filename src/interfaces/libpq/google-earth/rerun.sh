#!/bin/sh

####
#~ remember to add to cde.options
#~ ignore_environment_var=PTU_DB_MODE
#~ ignore_environment_var=PTU_DBSESSION_ID
#~ ignore_environment_var=PTU_DB_REPLAY

### start
export PTU_DB_REPLAY=/dblog.txt
export PTU_DB_MODE=22

# real re-run
cd cde-package
sh cde.log