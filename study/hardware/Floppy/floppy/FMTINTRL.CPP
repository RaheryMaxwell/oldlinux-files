#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <stdlib.h>
#include <bios.h>

// ����� �ଠ��㥬�� ��஦��
#define TRK 20

// ��� ࠧ��� ᥪ�� - 512 ����
#define SEC_SIZE 2

typedef struct _DPT_
{
  unsigned char srt_hut;
  unsigned char dma_hlt;
  unsigned char motor_w;
  unsigned char sec_size;
  unsigned char eot;
  unsigned char gap_rw;
  unsigned char dtl;
  unsigned char gap_f;
  unsigned char fill_char;
 unsigned char hst;
 unsigned char mot_start;
} DPT;

DPT far *get_dpt(void);

union REGS inregs, outregs;
char diskbuf[512];

int main(void)
{
  struct diskinfo_t di;
  unsigned status;
  unsigned char old_sec_size,
    old_fill_char, old_eot;
  int i, j;
  DPT  far *dpt_ptr;

  printf("\n�ணࠬ�� 㭨�⮦�� ᮤ�ন���"
    "\n20-� ��஦�� ��᪠ �:."
    "\n������ �த������? (Y,N)\n");

  i = getch();
  if((i != 'y') && (i != 'Y'))
    return(-1);

  // ����砥� ���� ⠡���� ��ࠬ��஢ ��᪥��
  dpt_ptr = get_dpt();

  // ���࠭塞 ���� ���祭�� �� ⠡���� ��ࠬ��஢
  old_sec_size  = dpt_ptr->sec_size;
  old_fill_char = dpt_ptr->fill_char;
  old_eot       = dpt_ptr->eot;

  // ��⠭�������� � ⠡��� ��ࠬ��஢ ��᪥��
  // ��� ࠧ��� ᥪ��, ᨬ��� ���������� ��
  // �ଠ�஢����, ������⢮ ᥪ�஢ �� ��஦��
  dpt_ptr->sec_size  = SEC_SIZE;
  dpt_ptr->fill_char = 0xf6;
  dpt_ptr->eot       = 18;

  // ��⠭�������� ⨯ ��᪠
  inregs.h.ah = 0x17;
  inregs.h.al = 3;
  inregs.h.dl = 0;
  int86(0x13, &inregs, &outregs);

  // ��⠭�������� �।� ��� �ଠ�஢����
  inregs.h.ah = 0x18;
  inregs.h.ch = TRK;
  inregs.h.cl = dpt_ptr->eot;
  inregs.h.dl = 0;
  int86(0x13, &inregs, &outregs);

  // �����⠢������ ��ࠬ����
  // ��� �㭪樨 �ଠ�஢����
  di.drive    = 0;
  di.head     = 0;
  di.track    = TRK;
  di.sector   = 1;
  di.nsectors = 18;
  di.buffer   = diskbuf;

  // �����⠢������ ���� �ଠ� ��� 18 ᥪ�஢
  // �ᯮ��㥬 ����� ���冷� �ᯮ������� ᥪ�஢
  // �� ��஦��
  for(i=0, j=18; j>0; i += 4, j--)
  {
    diskbuf[i]    = TRK;
    diskbuf[i+1]  = 0;
    diskbuf[i+2]  = j;
    diskbuf[i+3]  = SEC_SIZE;
  }

  // ��뢠�� �㭪�� �ଠ�஢���� ��஦��
  status = _bios_disk(_DISK_FORMAT, &di) >> 8;
  printf("\n��ଠ�஢���� �����訫��� � �����: %d",
    status);

  // ����⠭�������� ���� ���祭�� �
  // ⠡��� ��ࠬ��஢ ��᪥��
  dpt_ptr->sec_size  = old_sec_size;
  dpt_ptr->fill_char = old_fill_char;
  dpt_ptr->eot       = old_eot;

  return(0);
}

/**
* get_dpt
*
* ���᫨�� ���� ⠡���� ��ࠬ��஢ ��᪥��
*
* �㭪�� �����頥� 㪠��⥫� �� ⠡����
* ��ࠬ��஢ ��᪥��
*
**/
DPT far *get_dpt(void)
{
  void far * far *ptr;
  ptr = (void far * far *)MK_FP(0x0, 0x78);
  return(DPT far*)(*ptr);
}
