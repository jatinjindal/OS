#include "lib.h"

#define MAX_OBJS 1000000
#define Inode_data_start 289
#define Inode_data_end 31538 //31538
#define Inode_per_block 31
#define Inode_bitmap_start 0//0
#define Inode_bitmap_end 30//30
#define Data_bitmap_start 31
#define Data_bitmap_end 288
#define Data_start 31539

int lazy_allocated[17000] = {0};
struct object{
     long id;
     long size;
     int cache_index;
     int dirty;
     char key[32];
     long dir_pointer[4];
     long indir_pointer[4];
};

struct object *objs;

struct object *backup;
int* Data_bitmap;

char* Inodedata;
int* Inode_bitmap;
#define malloc_4kn(x,n) do{\
                         (x) = mmap(NULL, BLOCK_SIZE*n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_4kn(x,n) munmap((x), BLOCK_SIZE*n)


#define malloc_4k(x) do{\
                         (x) = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_4k(x) munmap((x), BLOCK_SIZE)

#ifdef CACHE         // CACHED implementation
static void init_object_cached(struct object *obj)
{
           obj->cache_index = -1;
           obj->dirty = 0;
           return;
}
static void remove_object_cached(struct object *obj)
{
           obj->cache_index = -1;
           obj->dirty = 0;
          return;
}
static int find_read_cached(struct objfs_state *objfs, struct object *obj, char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(user_buf, cache_ptr, size);
         return 0;
}
/*Find the object in the cache and update it*/
static int find_write_cached(struct objfs_state *objfs, struct object *obj, const char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(cache_ptr, user_buf, size);
         obj->dirty = 1;
         return 0;
}
/*Sync the cached block to the disk if it is dirty*/
static int obj_sync(struct objfs_state *objfs, struct object *obj)
{
   char *cache_ptr = objfs->cache + (obj->id << 12);
   if(!obj->dirty)
      return 0;
   if(write_block(objfs, obj->id, cache_ptr) < 0)
      return -1;
    obj->dirty = 0;
    return 0;
}
#else  //uncached implementation
static void init_object_cached(struct object *obj)
{
   return;
}
static void remove_object_cached(struct object *obj)
{
     return ;
}
static int find_read_cached(struct objfs_state *objfs, struct object *obj, char *user_buf, int size)
{
   void *ptr;
   malloc_4k(ptr);
   if(!ptr)
        return -1;
   if(read_block(objfs, obj->id, ptr) < 0)
       return -1;
   memcpy(user_buf, ptr, size);
   free_4k(ptr);
   return 0;
}
static int find_write_cached(struct objfs_state *objfs, struct object *obj, const char *user_buf, int size)
{
   void *ptr;
   malloc_4k(ptr);
   if(!ptr)
        return -1;
   memcpy(ptr, user_buf, size);
   if(write_block(objfs, obj->id, ptr) < 0)
       return -1;
   free_4k(ptr);
   return 0;
}
static int obj_sync(struct objfs_state *objfs, struct object *obj)
{
   return 0;
}
#endif

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
    struct object* obj = NULL;
    int ctr = 1;
    dprintf("entered into find object\n");

    // int* adsfa = (int*)Inode_bitmap;
    // dprintf("first char is %d key = %s\n",*adsfa,key);
    
    // dprintf("key in find = %s, id = %d\n",obj->key,obj->id);
    int bit = 1;
    while(ctr <= MAX_OBJS)
    {
        int x = *(Inode_bitmap + bit -1);
        for(int i=31;i>=0;i--)
        {
            //dprintf("check1 find\n");
            int temp =  x & (1<<i);
            // dprintf("Find x= %x, temp = %x",x,temp);
            if( temp !=0 ){
                // dprintf("temp = %d\n",temp);
            
                // dprintf("check2 find\n");
                int blk;
                int rem= 0;
                if(ctr % Inode_per_block == 0){
                blk = ctr/Inode_per_block;
                rem = Inode_per_block;
                }
                else{
                blk = ctr/Inode_per_block + 1;
                rem = ctr - (blk-1)*Inode_per_block;
                }


                obj = (struct object *) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
                // dprintf("check\n");
                // dprintf("ctr = %d\n",ctr);
                // dprintf("pass\n");
                if(obj->id && !strcmp(obj->key, key))
                {    
                    dprintf("found object id -> id = %ld, key= %s\n",obj->id,obj->key);
                    // dprintf("check\n");
                    backup = obj;
                    
                    return obj->id;
                }
                
            }
            ctr++;
        }
        bit++;
    }
    dprintf("Couldn't find the object\n");
    return -1;
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long create_object(const char *key, struct objfs_state *objfs)
{
    dprintf("entered into create_object\n");
    

    struct object* obj = NULL;
    int ctr = 1;
    int bit = 1;
    while(ctr<= MAX_OBJS)
    {
        int x = (Inode_bitmap[bit -1] );
        dprintf("x = %x\n",x);
        for(int i=31;i>=0;i--)
        {
            // dprintf("check1\n");
            int temp = x & (1<<i);
            if( temp == 0 ){
                dprintf("temp = %x\n",temp);
            
                // dprintf("check1\n");
                int blk;
                int rem= 0;
                if(ctr % Inode_per_block == 0){
                blk = ctr/Inode_per_block;
                rem = Inode_per_block;
                }
                else{
                blk = ctr/Inode_per_block + 1;
                rem = ctr - (blk-1) *Inode_per_block;
                }
                
                obj = (struct object*) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
                
                // dprintf("adfa %d",c);

                Inode_bitmap[bit-1] = x | 1<<i;
                int* tt = (int *)Inode_bitmap;
                dprintf("bitmap  = %x\n", *tt);
                // dprintf("bitmap  = %d\n", *tt );
                
                
                obj->id = ctr + 1;//Inode number + 1
                lazy_allocated[obj->id - 2] = 1;
                for(int i=0;i<4;i++)
                  obj->dir_pointer[i] = 0;

                for(int i=0;i<4;i++)
                  obj->indir_pointer[i] = 0;
                
                strcpy(obj->key, key);
                
                dprintf("Created Object---id = %ld, key = %s\n",obj->id,obj->key);
                // dprintf("ctr = %d\n",ctr);


                if(write_block(objfs, Inode_data_start + blk -1, Inodedata + (blk-1)*BLOCK_SIZE  ) < 0)
                    return -1;
                
                // init_object_cached(obj);
                dprintf("created the object successfully\n");
                return obj->id;
      
            }
            ctr++;
        }
        bit++;
    }
    dprintf("No space left\n");
    return -1;


}
/*
  One of the users of the object has dropped a reference
  Can be useful to implement caching.
  Return value: Success --> 0
                Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs)
{
    return 0;
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
long destroy_object(const char *key, struct objfs_state *objfs)
{
    dprintf("Deleting the object \n");
    long objid;
    if((objid = find_object_id(key, objfs)) < 0)  
        return -1;
   

    int blk;
    int rem= 0;
    int ctr = objid - 1;

    if(ctr % Inode_per_block == 0){
    blk = ctr/Inode_per_block;
    rem = Inode_per_block;
    }
    else{
    blk = ctr/Inode_per_block + 1;
    rem = ctr - (blk-1)*Inode_per_block;
    }
    struct object* obj = (struct object *) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
    
    ctr--;
    int num = ctr/32;
    int c = Inode_bitmap[num];
    int r = 31 - ctr%32;
    unsigned int x = 1 << r;
    Inode_bitmap[num] =  c ^ x;
    dprintf("Deleting the inode at blk = %d, at bit= %d\n",ctr,r);
    dprintf("Inode_bitmap = %x",Inode_bitmap[num]);
    obj->id = 0;
    int total_size = obj->size;
    obj->size = 0;
    lazy_allocated[ctr] = 1;
    for(int i=0;i<4;i++)
    {
      
      ctr = obj->dir_pointer[i];
   
      // ctr++;
      num = (ctr)/32;
      c = Data_bitmap[num];
      r = 31 - ctr%8;
      x = 1 << r;
      Data_bitmap[num] =  c ^ x;

      dprintf("Deleting the direct data at blk = %d, at bit= %d\n",ctr,r);
      dprintf("Data_bitmap = %x",Data_bitmap[num]);
    
      total_size = total_size - BLOCK_SIZE;
      if(total_size<=0)
        break;
    }

    for(int i=0;i<4;i++)
    {
      if(total_size<=0)
        break;
      ctr = obj->indir_pointer[i];

      num = ctr/32;
      c = Data_bitmap[num];
      r = 31 - ctr%32;
      x = 1 << r;
      Data_bitmap[num] =  c ^ x;
      dprintf("Deleting the indirect pointer block at blk = %d, at bit= %d\n",ctr,r);
      dprintf("Data_bitmap = %x",Data_bitmap[num]);
    
      char* temp_buf;
      malloc_4k(temp_buf);
      if(read_block(objfs,Data_start + obj->indir_pointer[i], temp_buf) < 0)
          return -1;

      int *st = (int *)temp_buf;
      for(int offset=0; offset<1024;i++)
      {
        if(total_size<=0)
          break;

        int cur_blk = *(st + offset);
        
        num = cur_blk/32;
        c = Data_bitmap[num];
        r = 31 - cur_blk%32;
        x = 1 << r;
        Data_bitmap[num] =  c ^ x;
        dprintf("Deleting the indirect data at blk = %d, at bit= %d\n",cur_blk,r);
        dprintf("Inode_bitmap = %x",Data_bitmap[num]);
    
        total_size = total_size - BLOCK_SIZE;

      }

      free_4k(temp_buf);
    }
    return 0;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.  
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
   long objid;
   if((objid = find_object_id(key, objfs)) < 0)  
        return -1;
   

    int blk;
    int rem= 0;
    int ctr = objid - 1;
    lazy_allocated[ctr-1] = 1;
    if(ctr % Inode_per_block == 0){
    blk = ctr/Inode_per_block;
    rem = Inode_per_block;
    }
    else{
    blk = ctr/Inode_per_block + 1;
    rem = ctr - (blk-1)*Inode_per_block;
    }
    struct object* obj = (struct object *) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
    
   if(strlen(newname) > 32)
      return -1;
   strcpy(obj->key, newname);
   // obj->dirty = 1;
   return 0;
}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, int offset, struct objfs_state *objfs)
{
    dprintf("entered into write\n");
   
    int blk;
    int rem= 0;
    int ctr = objid - 1;

    if(ctr % Inode_per_block == 0){
    blk = ctr/Inode_per_block;
    rem = Inode_per_block;
    }
    else{
    blk = ctr/Inode_per_block + 1;
    rem = ctr - (blk-1)*Inode_per_block;
    }
    //inode of the file
    struct object* obj = (struct object *) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
                
    if(obj->id != objid)
       return -1;
    
    dprintf("Doing write size = %d\n", size);
    





    blk =0;//byte to read
    ctr =0;//current block position from Data_start
    int x = 1;
    int file_block = offset/BLOCK_SIZE;

    // int size_written = 0;
    // int ptr_index=0;
    lazy_allocated[obj->id-2] = 1;
    
    //if the block already exists
    if(obj->size > offset)
    {
      if(file_block < 4)
      {
        char * temp_buf;
        malloc_4k(temp_buf);
        memcpy(temp_buf,buf,BLOCK_SIZE);
        
        if(write_block(objfs,Data_start + obj->dir_pointer[file_block], temp_buf) < 0)
          return -1;
        free_4k(temp_buf);
        dprintf("Written in block = %d, id = %ld, key = %s\n",ctr,obj->id,obj->key);
        return size;
      }
      else
      {
        int index = (file_block-4)/1024;
        char * temp_buf;
        malloc_4k(temp_buf);
        memcpy(temp_buf,buf,BLOCK_SIZE);
        
        if(write_block(objfs,Data_start + obj->indir_pointer[index], temp_buf) < 0)
          return -1;
        free_4k(temp_buf);
        dprintf("Written in block = %d, id = %ld, key = %s\n",ctr,obj->id,obj->key);
        return size;

      }
    }            

    //if it doesn't exist
    blk =0 ;
    while(1)
    {
        int c = Data_bitmap[blk];
    
        for(int i=31;i>=0;i--)
        {
            if ( (c & (1<<i) ) == 0 )
            {

                if(file_block < 4){

                  char * temp_buf;
                  malloc_4k(temp_buf);

                  memcpy(temp_buf,buf,BLOCK_SIZE);
                  
                  // dprintf("temp_buf = %s\n",temp_buf);
                  if(write_block(objfs,Data_start + ctr, temp_buf) < 0)
                    return -1;
                  free_4k(temp_buf);
                  dprintf("Block doesn't exist, Written in block = %d, id = %ld, key = %s\n",ctr,obj->id,obj->key);
                  c = c | 1<<i;
                  Data_bitmap[blk] = c;
                  dprintf("Data_bitmap = %x\n",Data_bitmap[blk]);
                  obj->dir_pointer[file_block] = ctr;
                  obj->size = offset + size;
                  return size;
               
                }
                else{

                    int index = (file_block-4)/1024;
                    if(obj->indir_pointer[index] == 0)
                    {
                      c = c | 1<<i;
                      Data_bitmap[blk] = c;
                      dprintf("Data_bitmap = %x\n",Data_bitmap[blk]);
                      obj->indir_pointer[index] = ctr;
                      dprintf("Indirect pointer block= %d, id = %ld, key = %s\n",ctr,obj->id,obj->key);
                      
                    }
                    else
                    {
                      char * temp_buf;
                      malloc_4k(temp_buf);
                      memcpy(temp_buf,buf,BLOCK_SIZE);
                      
                      if(write_block(objfs,Data_start + ctr, temp_buf) < 0)
                        return -1;
                      
                      if(read_block(objfs,Data_start + obj->indir_pointer[index], temp_buf) < 0)
                        return -1;

                      int* st = (int *)temp_buf;
                      st = st + file_block - 4 - index*1024;
                      *st = ctr;


                      if(write_block(objfs,Data_start + obj->indir_pointer[index], temp_buf) < 0)
                        return -1;


                      if(read_block(objfs,Data_start + obj->indir_pointer[index], temp_buf) < 0)
                        return -1;
                      // int * asd = temp_buf;
                      // dprintf("temp_buf == %d", *asd ) ;
                      free_4k(temp_buf);
                      dprintf("Block in Indirect Pointer = %d, id = %ld, key = %s\n",ctr,obj->id,obj->key);
                      c = c | 1<<i;
                      Data_bitmap[blk] = c;

                      dprintf("Data_bitmap = %x\n",Data_bitmap[blk]);
                      obj->size = offset + size;
                      return size;
                    }
                
                }

                
            }
            ctr++;

        }
        blk++;

          
    }
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size,int offset, struct objfs_state *objfs)
{
   dprintf("entered into read\n");

    int blk;
    int rem= 0;
    int ctr = objid - 1;

    if(ctr % Inode_per_block == 0){
    blk = ctr/Inode_per_block;
    rem = Inode_per_block;
    }
    else{
    blk = ctr/Inode_per_block + 1;
    rem = ctr - (blk-1)*Inode_per_block;
    }
    //inode of the file
    struct object* obj = (struct object *) (Inodedata + (blk-1)*BLOCK_SIZE + (rem-1)* (sizeof(struct object) ) );
                
    if(obj->id != objid)
       return -1;
    
    
    dprintf("Doing read size = %d\n", size);
    int bytes_read =0 ;
    while(bytes_read < size)
    {
      int file_block = offset/BLOCK_SIZE;
      if(file_block<4)
      {
        blk = obj->dir_pointer[file_block];
        char * temp_buf;
        malloc_4k(temp_buf);
        if(read_block(objfs,Data_start + blk, temp_buf) < 0)
            return -1;
        memcpy(buf + bytes_read,temp_buf ,BLOCK_SIZE);
        // dprintf("buf = %s------------------\n",temp_buf);

        dprintf("Read into block = %d, id = %ld, key = %s\n",blk,obj->id,obj->key);
        free_4k(temp_buf);
      }
      else{
        int index = (file_block-4)/1024;
        int blk_to_read = obj->indir_pointer[index];
        dprintf("Block-to-read = %d\n",blk_to_read);
        int remaining = file_block - 4 - index*1024;

        char * temp_buf;
        malloc_4k(temp_buf);
        if(read_block(objfs,Data_start + blk_to_read, temp_buf) < 0)
            return -1;
        // int *asd = temp_buf;
        // dprintf("Indirect pointer blk data = %d\n", *(asd));
        int* st = (int *)temp_buf;
        // dprintf("Remaining = %d\n",remaining);
        st = st + remaining;

        dprintf("Read into block = %d, id = %ld, key = %s\n",*st,obj->id,obj->key);
        
        if(read_block(objfs,Data_start + (*st), temp_buf) < 0)
            return -1;

        memcpy(buf + bytes_read,temp_buf ,BLOCK_SIZE);
        // dprintf("buf = %s------------------\n",buf);

        free_4k(temp_buf);
        

      }
      bytes_read = bytes_read + BLOCK_SIZE;
      offset = offset + BLOCK_SIZE;
    }

    
   // if(find_read_cached(objfs, obj, buf, size) < 0)
   //     return -1; 
   return size;
}

/*
  Reads the object metadata for obj->id = objid.
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf)
{
   struct object *obj = backup;
   if(buf->st_ino < 2 || obj->id != buf->st_ino)
       return -1;
   buf->st_size = obj->size;
   buf->st_blocks = obj->size >> 9;
   if(((obj->size >> 9) << 9) != obj->size)
       buf->st_blocks++;
   return 0;
}

/*
   Set your private pointeri, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
   int ctr;
   ctr = Inode_bitmap_start;
    char *temp_buf;
    malloc_4k(temp_buf);
    if(read_block(objfs, ctr, temp_buf) < 0)
       return -1;
    
    int *temp = temp_buf;
    dprintf("ctr = %d, Inode bitmap = %x\n",ctr,*temp);
    free_4k(temp_buf);


   struct object *obj = NULL;
   malloc_4k(objs);
   malloc_4k(backup);
   if(!objs){
       dprintf("%s: malloc\n", __func__);
       return -1;
   }

   if(read_block(objfs, 0, (char *)objs) < 0)
       return -1;

    malloc_4kn(Inodedata, Inode_data_end- Inode_data_start + 1 );
    malloc_4kn(Inode_bitmap, Inode_bitmap_end- Inode_bitmap_start + 1 );
    
    malloc_4kn(Data_bitmap, Data_bitmap_end- Data_bitmap_start + 1 );


    int offset = 0;
    for(ctr=Inode_data_start;ctr<=Inode_data_end;ctr++,offset++)
    {
      if(read_block(objfs, ctr, Inodedata + offset*BLOCK_SIZE) < 0)
       return -1;
    }

    for(ctr=Inode_bitmap_start;ctr<=Inode_bitmap_end;ctr++)
    {
      if(read_block(objfs, ctr, (char *)Inode_bitmap +  (ctr - Inode_bitmap_start)*BLOCK_SIZE ) < 0)
       return -1;
    }

    offset = 0;
    for(ctr=Data_bitmap_start;ctr<=Data_bitmap_end;ctr++,offset++)
    {
      if(read_block(objfs, ctr, (char *)Data_bitmap + offset*BLOCK_SIZE) < 0)
       return -1;
    }



   // obj = objs;
   // for(ctr=0; ctr < MAX_OBJS; ctr++, obj++){
   //    if(obj->id)
   //        init_object_cached(obj);
   // }
  
  struct object* obj1 = (struct object *)Inodedata;
  dprintf("key= %s, id=%ld\n",obj1->key, obj1->id);
  
  temp = (int *)Inode_bitmap;
  dprintf("bitmap = %x\n",*temp);

    
    ctr = Data_bitmap_start;
    malloc_4k(temp_buf);
    if(read_block(objfs, ctr, temp_buf) < 0)
       return -1;
    
    temp = temp_buf;
    dprintf("Data bitmap = %x\n",*temp);
    free_4k(temp_buf);


   objfs->objstore_data = objs;
   dprintf("Done objstore init\n");
   return 0;
} 

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
   

   int ctr;
   for(ctr =0; ctr<17000;ctr++)
    {
      
      // dprintf("Lazily allocated %d\n",lazy_allocated[0]);
      if(lazy_allocated[ctr]==1)
      {
        dprintf("Lazily allocated blocks = %d\n",ctr);
        if(write_block(objfs, Inode_data_start + ctr, Inodedata + ctr*BLOCK_SIZE) < 0)
          return -1;
      }
    }
    // struct object* obj1 = (struct object *)Inodedata;
    // dprintf("key= %s, id=%d\n",obj1->key, obj1->id);

    int *temp = Inode_bitmap;
    dprintf("Inode bitmap = %x\n",*temp);

    for(ctr=Inode_bitmap_start; ctr<=Inode_bitmap_end;ctr++)
    {
        if(write_block(objfs, ctr, (char*)Inode_bitmap + (ctr - Inode_bitmap_start)*BLOCK_SIZE ) < 0)
          return -1;
    }
    
    // temp = Data_bitmap;
    // dprintf("Data_bitmap = %x\n",*temp);
    for(ctr = Data_bitmap_start;ctr<=Data_bitmap_end;ctr++)
    {
      if(write_block(objfs, ctr, (char*)Data_bitmap +(ctr- Data_bitmap_start) *BLOCK_SIZE ) < 0)
      return -1;
    }
   // struct object *obj = objs;
   // for(ctr=0; ctr < MAX_OBJS; ctr++, obj++){
   //          if(obj->id)
   //              obj_sync(objfs, obj);
   // }
  
   free_4kn(Inode_bitmap,Inode_bitmap_end- Inode_bitmap_start + 1 );
   free_4kn(Data_bitmap,Data_bitmap_end- Data_bitmap_start + 1);
   free_4kn(Inodedata, Inode_data_end - Inode_data_start + 1);
   objfs->objstore_data = NULL;
   

    ctr = Inode_bitmap_start;
    char *temp_buf;
    malloc_4k(temp_buf);
    if(read_block(objfs, ctr, temp_buf) < 0)
       return -1;
    
    temp = temp_buf;
    dprintf("ctr = %d,Inode bitmap = %x\n",ctr,*temp);
    free_4k(temp_buf);


   dprintf("Done objstore destroy\n");
   return 0;
}
