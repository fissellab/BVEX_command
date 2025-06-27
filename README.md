BVEX_command

Put flight_code_oph to Ophiuchus, it contains the other subsystems' code
Put the content of flight_code_sag to a folder containing the rest of the subsystems' code. Replace the original cli_sag.c and cli_sag.h with the two new files from flight_code_sag

flight_code_ground can run as is. It only translates commands into packets

Need to add server interaction to the listen() and send() functions in cli_Oph.c cli_sag.c cli_ground.c, located in their respoective folder

For more details, the comment at the beginning of cli_Oph.c in flight_code_oph should explain how the code works

NOTICE.md in flight_code_oph CONTAINS AN IMPORTANT REMINDER
