/* File Server Emulator   */
/* fsem.h                 */
/* (c) 2021 Martin Mather */

int fsem_open(char *fname, uint32_t loadaddr, uint16_t stn, char *scsiname);
void fsem_close(void);
void fsem_sendkey(char key);
int fsem_exec(int opcount, int jsr);
