      subroutine test
!pragma array1
      integer,parameter :: vecsize = 10
      real(8),dimension(vecsize):: array1
      do i=1,vecsize
        array1(i) = 1.
      enddo
      end subroutine test
