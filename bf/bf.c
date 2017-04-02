#include <stdio.h>
#include "bfHeader.h"
#include "lru.h"
#include "hashtable.h"
#include "freeList.h"

/*
 * Constants freelist, hashtable, lru
 * Waiting for an answer of the teacher (question was : can we add some code into the headers he provided us ?)
 */ 

Freelist* fl;
LRU* lru;
Hashtable* ht;


/*
 * Init the BF layer
 * Creates all the buffer entries and add them to the freelist
 * Also init the hashtable
 * Dev : Antoine
 */
void BF_Init(void){
	fl = fl_init(BF_MAX_BUFS); //buffer entries malloc in the fl_function
	lru = lru_init();
	ht = ht_init(BF_HASH_TBL_SIZE);
}

/*
 * Unlike BF_GetBuf(), this function is used in situations where a new page is 
 * added to a file. Thus, if there already exists a buffer page in the pool associated
 * with a PF file descriptor and a page number passed over in the buffer control 
 * block bq, a PF error code must be returned. Otherwise, a new buffer page should 
 * be allocated (by page replacement if there is no free page in the buffer pool). 
 * Then, its pin count is set to one, its dirty flag is set to FALSE, other appropriate
 * fields of the BFpage structure are set accordingly, and the page becomes the most
 * recently used. Note that it is not necessary to read anything from the file into 
 * the buffer page, because a brand new page is being appended to the file. This function
 * returns BFE_OK if the operation is successful, an error condition otherwise.
 * Dev : Patric
 */
int BF_AllocBuf(BFreq bq, PFpage **fpage){
	BFhash_entry* e = ht_get(ht, &fpage->fd, &fpage->pageNum);
	if (e != NULL) {
		return BFE_PAGEINBUF; /* it is a new page, so it must not be in buffer yet */
	}
	BFpage* page = fl_give_one(fl);
	if (page == NULL) { /* there is no free page, need to replace one (aka find victim) */
		page = lru_remove(lru);
	}

	page->count = 1;
	page->dirty = FALSE;
	page->prevpage = NULL;
	page->nextpage = NULL;
	/*page->fpage = &&fpage; ?????? TODO*/
	page->pageNum = bq.pagenum;
	page->fd = bq.fd; /* TODO or unixfd? */

	ht_add(ht, page);
	return lru_add(lru, page);
}

/*
 * Returns a PF page in a memory block pointed to by *fpage
 * Dev : Antoine
 */
int BF_GetBuf(BFreq bq, PFpage **fpage){
	BFhash_entry* h_entry;
	BFpage* bfpage_entry;
	BFpage* victim;
	int res;

	h_entry = ht_get(ht, bq->fd, bq->pagenum);

	//page already in buffer
	if(h_entry != NULL){
		//to check
		h_entry->count += 1;
		&fpage = h_entry->fpage;
		return BFE_OK;
	}


	//page not in buffer
	else{
		bfpage_entry = fl_give_one(fl);

		//No more  place in the buffer <=> No more freespace
		if( bfpage_entry == NULL){
			
			//find a victim
			res = lru_remove(lru, &victim);
			
			if(lru_remove(lru, &victim) != 0){return res;} 	//no victim found
			else											//victim found
			{
				if(victim->dirty == TRUE)					//victim dirty : try to flush it, trhow error otherwise
				{
					if(pwrite(victim->unixfd, victim->fpage->pagebuf, PAGE_SIZE, ((victim->pagenum)-1)*PAGE_SIZE) != PAGE_SIZE){
						return BFE_INCOMPLETEWRITE;
					}
				}
				//remove victim
				ht_remove(ht, victim);
				fl_add(fl, victim);
			}

			bfpage_entry = fl_give_one(fl);
		}
		
		//space available in buffer(add page to LRU and HT)
		if(lru_add(lru, bfpage_entry) == BFE_OK && ht_add(ht, bfpage_entry) == BFE_OK){
		}else{return BFE_PAGENOTOBUF;}


		//try to read the file asked
		if(fread(bq->unixfd, bfpage_entry->fpage->pagebuf, PAGE_SIZE, ((bq->pagenum)-1)*PAGE_SIZE) == -1){
			return BFE_INCOMPLETEREAD;
		}

		//set the correct parameters
		bfpage_entry->count = 1;
		bfpage_entry->dirty = FALSE;
		bfpage_entry->fd = bq->fd;
		bfpage_entry->pagenum = bq->pagenum;
		bfpage_entry->unixfd = bq->unixfd;

		//value returned to the user
		&fpage = bfpage_entry->fpage;
		return BFE_OK;
	
	}
}


/*
 * This function unpins the page whose identification is passed over in the 
 * buffer control block bq. This page must have been pinned in the buffer already.
 * Unpinning a page is carried out by decrementing the pin count by one. 
 * This function returns BFE_OK if the operation is successful, an error condition otherwise.
 * Dev : Patric
 */
int BF_UnpinBuf(BFreq bq){
	BFPage* page = ht_get(ht, bq.fd, bq.pagenum);
	if (page == NULL || page->count < 1) { /* must be pinned */
		return -1; /*TODO return what?*/
	}
	page->count = page->count - 1;
	return BFE_OK;
}

/*
 * This function marks the page identified by the buffer control block bq as dirty.
 * The page must have been pinned in the buffer already. The page is also made the
 * most recently used by moving it to the head of the LRU list. This function
 * returns BFE_OK if the operation is successful, an error condition otherwise.
 * Dev : Paul
 */
int BF_TouchBuf(BFreq bq){
    BFpage* page;

    /* pointer on the page is get by using hastable */
    page=(ht_get(ht, bq.fd, bq.pagenum))->bpage;

    /* page has to be pinned */
    if(page==NULL) return BFE_HASHNOTFOUND;
    if(page->count==0) return BFE_UNPINNEDPAGE;
    
    /* page is marked as dirty */
    page->dirty=TRUE;  //???????????? TRUE or bq.dirty? /////////////////////////////////////////////////////////////
    
    /* page has to be head of the list */
    return lru_mtu(lru, page);
}

/*
 * Dev : Paul
 */
int BF_FlushBuf(int fd){
	

}

/*
 * Dev : Paul
 */
int BF_ShowBuf(void){
	return lru_print(lru);

}


