/*
 * pin_mapper.h
 *
 *  Created on: Apr 19, 2012
 *      Author: hendrix
 */

#ifndef PIN_MAPPER_H_
#define PIN_MAPPER_H_


void* CreateMapFile(char *filepath, unsigned long bytes);
char* Writeline(unsigned long strt, unsigned long end);
char *CreateSharedMem();


#endif /* PIN_MAPPER_H_ */
