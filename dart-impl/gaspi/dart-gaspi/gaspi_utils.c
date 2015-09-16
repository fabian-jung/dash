#include "gaspi_utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static gaspi_segment_id_t gaspi_utils_seg_counter;

void delete_all_segments() {
  while (gaspi_utils_seg_counter-- > 0) {
    DART_CHECK_ERROR(gaspi_segment_delete(gaspi_utils_seg_counter));
  }
}

gaspi_return_t create_segment(const gaspi_size_t size,
                              gaspi_segment_id_t *seg_id) {
  gaspi_return_t retval = GASPI_SUCCESS;
  gaspi_number_t seg_max = 0;
  DART_CHECK_ERROR(gaspi_segment_max(&seg_max));

  if (seg_max < gaspi_utils_seg_counter) {
    gaspi_printf("Error: Can't create segment, reached max. of segments\n");
    return GASPI_ERROR;
  }

  DART_CHECK_ERROR_RET(retval, gaspi_segment_create(gaspi_utils_seg_counter, size,
                                                      GASPI_GROUP_ALL, GASPI_BLOCK,
                                                      GASPI_MEM_UNINITIALIZED));
  *seg_id = gaspi_utils_seg_counter;

  gaspi_utils_seg_counter++;

  return retval;
}

void check_queue_size(gaspi_queue_id_t queue) {
  gaspi_number_t queue_size = 0;
  DART_CHECK_ERROR(gaspi_queue_size(queue, &queue_size));

  gaspi_number_t queue_size_max = 0;
  DART_CHECK_ERROR(gaspi_queue_size_max(&queue_size_max));

  if (queue_size >= queue_size_max) {
    DART_CHECK_ERROR(gaspi_wait(queue, GASPI_BLOCK));
  }
}

void wait_for_queue_entries(gaspi_queue_id_t *queue,
                            gaspi_number_t wanted_entries) {
  gaspi_number_t queue_size_max;
  gaspi_number_t queue_size;
  gaspi_number_t queue_num;

  DART_CHECK_ERROR(gaspi_queue_size_max(&queue_size_max));
  DART_CHECK_ERROR(gaspi_queue_size(*queue, &queue_size));
  DART_CHECK_ERROR(gaspi_queue_num(&queue_num));

  if (!(queue_size + wanted_entries <= queue_size_max)) {
    *queue = (*queue + 1) % queue_num;
    DART_CHECK_ERROR(gaspi_wait(*queue, GASPI_BLOCK));
  }
}

void blocking_waitsome(gaspi_notification_id_t id_begin,
                       gaspi_notification_id_t id_count,
                       gaspi_notification_id_t *id_available,
                       gaspi_notification_t *notify_val,
                       gaspi_segment_id_t seg) {
  DART_CHECK_ERROR(gaspi_notify_waitsome(seg, id_begin, id_count,
                                           id_available, GASPI_BLOCK));
  DART_CHECK_ERROR(gaspi_notify_reset(seg, *id_available, notify_val));
}

void flush_queues(gaspi_queue_id_t queue_begin, gaspi_queue_id_t queue_count) {
  gaspi_number_t queue_size = 0;

  for (gaspi_queue_id_t queue = queue_begin;
       queue < (queue_count + queue_begin); ++queue) {
    DART_CHECK_ERROR(gaspi_queue_size(queue, &queue_size));

    if (queue_size > 0) {
      DART_CHECK_ERROR(gaspi_wait(queue, GASPI_BLOCK));
    }
  }
}

/************************************************************************************
 *
 * collective operations helper functions
 *
 ************************************************************************************/
static unsigned int npot(unsigned int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;

  return v + 1;
}

unsigned int gaspi_utils_get_bino_num(int rank, int root, int rank_count) {
  rank -= root;
  if (rank < 0) {
    rank += rank_count;
  }
  return rank;
}

unsigned int gaspi_utils_get_rank(int relative_rank, int root, int rank_count) {
  relative_rank += root;
  if (relative_rank >= rank_count) {
    relative_rank -= rank_count;
  }
  return relative_rank;
}

int gaspi_utils_compute_comms(int *parent, int **children, int me, int root) {
  gaspi_rank_t size;

  DART_CHECK_ERROR(gaspi_proc_num(&size));
  unsigned int size_pot = npot(size);

  unsigned int d;
  unsigned int number_of_children = 0;

  /* Be your own parent, ie. the root, by default */
  *parent = me;

  me = gaspi_utils_get_bino_num(me, root, size);

  /* Calculate the number of children for me */
  for (d = 1; d; d <<= 1) {
    /* Actually break condition */
    if (d > size_pot) {
      break;
    }

    /* Check if we are actually a child of someone */
    if (me & d) {
      /* Yes, set the parent to our real one, and stop */
      *parent = me ^ d;
      *parent = gaspi_utils_get_rank(*parent, root, size);
      break;
    }

    /* Only count real children, of the virtual hypercube */
    if ((me ^ d) < size) {
      number_of_children++;
    }
  }

  /* Put the ranks of all children into a list and return */
  *children = malloc(sizeof(**children) * number_of_children);
  unsigned int child = number_of_children;

  d >>= 1;
  while (d) {
    if ((me ^ d) < size) {
      (*children)[--child] = me ^ d;
      (*children)[child] = gaspi_utils_get_rank((*children)[child], root, size);
    }
    d >>= 1;
  }

  return number_of_children;
}
/************************************************************************************
 *
 * collective operations helper functions
 *
 ************************************************************************************/
/**
 * blocking operation for GASPI_GROUP_ALL
 * @param seg_id:   same segement for all participating processes
 * @param offset:   same offset for all participating processes
 * @param root:     Rank number of the root processes
 * @param bytesize: Size in bytes of the data
 */
gaspi_return_t gaspi_bcast(gaspi_segment_id_t seg_id, gaspi_offset_t offset,
                           gaspi_size_t bytesize, gaspi_rank_t root) {
  int children_count;
  int child;
  gaspi_rank_t rank;
  gaspi_rank_t rankcount;
  gaspi_return_t retval = GASPI_SUCCESS;
  gaspi_pointer_t p_segment = NULL;
  gaspi_notification_id_t notify_id = 0;
  gaspi_queue_id_t queue = 0;
  gaspi_notification_id_t first_id;
  gaspi_notification_t old_value;

  int parent;
  int *children = NULL;

  DART_CHECK_ERROR_RET(retval, gaspi_proc_rank(&rank));
  DART_CHECK_ERROR_RET(retval, gaspi_proc_num(&rankcount));

  DART_CHECK_ERROR_RET(retval, gaspi_segment_ptr(seg_id, &p_segment));
  p_segment = p_segment + offset;

  children_count = gaspi_utils_compute_comms(&parent, &children, rank, root);

  DART_CHECK_ERROR_RET(retval, gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK));
  /*
   * parents + children wait for upper parents data
   */
  if (rank != parent)
  {
    blocking_waitsome(notify_id, 1, &first_id, &old_value, seg_id);
  }
  /*
   * write to all childs
   */
  for (child = 0; child < children_count; child++)
  {
    check_queue_size(queue);
    DART_CHECK_ERROR_RET(retval, gaspi_write_notify(seg_id, offset, children[child],
                             seg_id, offset, bytesize, notify_id,
                             42, queue, GASPI_BLOCK));
  }

  free(children);
  DART_CHECK_ERROR_RET(retval, gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK));
  return retval;
}

gaspi_return_t gaspi_bcast_asym(gaspi_segment_id_t seg_id,
                                gaspi_offset_t offset, gaspi_size_t bytesize,
                                gaspi_segment_id_t transfer_seg_id,
                                gaspi_rank_t root)

{
  gaspi_rank_t iProc;
  gaspi_pointer_t transfer_ptr = NULL;
  gaspi_pointer_t src_ptr = NULL;
  gaspi_return_t  retval = GASPI_SUCCESS;
  DART_CHECK_ERROR_RET(retval, gaspi_proc_rank(&iProc));
  DART_CHECK_ERROR_RET(retval, gaspi_segment_create(transfer_seg_id, bytesize,
                                          GASPI_GROUP_ALL, GASPI_BLOCK,
                                          GASPI_MEM_UNINITIALIZED));

  DART_CHECK_ERROR_RET(retval, gaspi_segment_ptr(transfer_seg_id, &transfer_ptr));
  DART_CHECK_ERROR_RET(retval, gaspi_segment_ptr(seg_id, &src_ptr));

  if (iProc == root) {
    memcpy(transfer_ptr, src_ptr + offset, bytesize);
  }

  DART_CHECK_ERROR_RET(retval, gaspi_bcast(transfer_seg_id, 0UL, bytesize, root));

  if (iProc != root) {
    memcpy(src_ptr + offset, transfer_ptr, bytesize);
  }

  DART_CHECK_ERROR_RET(retval, gaspi_segment_delete(transfer_seg_id));

  return retval;
}
/**
 * TODO rename
 */
int cmp_ranks(const void * a, const void * b)
{
    const int c = *((gaspi_rank_t *) a);
    const int d = *((gaspi_rank_t *) b);
    return c - d;
}

gaspi_return_t
gaspi_allgather(const gaspi_segment_id_t send_segid,
                const gaspi_offset_t     send_offset,
                const gaspi_segment_id_t recv_segid,
                const gaspi_offset_t     recv_offset,
                const gaspi_size_t       byte_size,
                const gaspi_group_t      group)
{
    gaspi_rank_t            rank;
    gaspi_return_t          retval    = GASPI_SUCCESS;
    gaspi_queue_id_t        queue     = 0;

    DART_CHECK_ERROR_RET(retval, gaspi_barrier(group, GASPI_BLOCK));

    gaspi_proc_rank(&rank);

    gaspi_number_t group_size;

    DART_CHECK_ERROR_RET(retval, gaspi_group_size(group, &group_size));

    gaspi_rank_t * ranks = (gaspi_rank_t *) malloc(sizeof(gaspi_rank_t) * group_size);
    assert(ranks);
    DART_CHECK_ERROR_RET(retval, gaspi_group_ranks(group, ranks));

    qsort(ranks, group_size, sizeof(gaspi_rank_t), cmp_ranks);

    int rel_rank = -1;
    for(unsigned int i = 0; i < group_size; ++i)
    {
        if ( ranks[i] == rank )
        {
            rel_rank = i;
            break;
        }
    }

    if(rel_rank == -1)
    {
        fprintf(stderr, "Error: rank %d is no member of group %d", rank, group);
    }

    for (unsigned int i = 0; i < group_size; i++)
    {
        if ( ranks[i] == rank )
        {
            continue;
        }

        check_queue_size(queue);

        DART_CHECK_ERROR_RET(retval, gaspi_write_notify(send_segid,
                                                        send_offset,
                                                        ranks[i],
                                                        recv_segid,
                                                        recv_offset + (rel_rank * byte_size),
                                                        byte_size,
                                                        (gaspi_notification_id_t) rel_rank,
                                                        42,
                                                        queue,
                                                        GASPI_BLOCK));
        gaspi_wait(queue, GASPI_BLOCK);
    }
    int missing = group_size - 1;
    gaspi_notification_id_t id_available;
    gaspi_notification_t    id_val;
    gaspi_pointer_t recv_ptr;
    gaspi_pointer_t send_ptr;

    DART_CHECK_ERROR_RET(retval, gaspi_segment_ptr(send_segid, &send_ptr));
    DART_CHECK_ERROR_RET(retval, gaspi_segment_ptr(recv_segid, &recv_ptr));

    recv_ptr = (void *) ((char *) recv_ptr + recv_offset + (rel_rank * byte_size));
    memcpy(recv_ptr, send_ptr, byte_size);

    while(missing-- > 0)
    {
        blocking_waitsome(0, group_size, &id_available, &id_val, recv_segid);
        if(id_val != 42)
        {
            fprintf(stderr, "Error: Get wrong notify in allgather on rank %d\n", rank);
        }
    }

    free(ranks);

    DART_CHECK_ERROR_RET(retval, gaspi_barrier(group, GASPI_BLOCK));

    return retval;
}
