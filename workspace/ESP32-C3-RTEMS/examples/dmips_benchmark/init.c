/*
 * Dhrystone 2.1 DMIPS benchmark for RTEMS on the ESP32-C3 (esp32c3db BSP),
 * result printed over the console (UART0/USB-Serial).
 *
 * Console-only, like ../hello_world - no GPIO driver dependency, so this
 * builds and runs against the stock esp32c3db BSP.
 *
 * The Dhrystone 2.1 algorithm below (types, globals, Proc_1..8, Func_1..3,
 * and the measurement loop) is ported near-verbatim from the public-domain
 * Dhrystone 2.1 C sources (Reinhold P. Weicker, 1988) as vendored in this
 * repo under workspace/Luckfoxpico/luckfox-pico/sysdrv/source/uboot/u-boot/
 * lib/dhry/ (dhry.h, dhry_1.c, dhry_2.c) - merged into one file and with the
 * "register" storage class dropped (the benchmark's own default per its
 * header comment: "Default results are those without register
 * declarations"). Unlike that UNIX version, timing isn't done inside dhry()
 * itself; Init() below times it with RTEMS's clock and reports Dhrystones/
 * second and DMIPS (Dhrystones/second / 1757, the VAX 11/780 reference used
 * industry-wide since Dhrystone's original publication), the same
 * calculation U-Boot's own "dhry" command (lib/dhry/cmd_dhry.c) does around
 * the same dhry() entry point.
 *
 * Number_Of_Runs is only known at runtime - found below by a calibration
 * ramp sized off the wall clock, so it works regardless of actual core
 * clock speed. Because it's a genuine runtime value, the compiler cannot
 * constant-fold or dead-code-eliminate the loop inside dhry() even at -O2,
 * so no volatile globals or -fno-inline tricks are needed to keep the
 * benchmark honest.
 */

#include <rtems.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ---- dhry.h: types ---- */

typedef enum {
  Ident_1, Ident_2, Ident_3, Ident_4, Ident_5
} Enumeration;

typedef int  One_Thirty;
typedef int  One_Fifty;
typedef char Capital_Letter;
typedef int  Boolean;
typedef char Str_30[31];
typedef int  Arr_1_Dim[50];
typedef int  Arr_2_Dim[50][50];

typedef struct record {
  struct record *Ptr_Comp;
  Enumeration    Discr;
  union {
    struct {
      Enumeration Enum_Comp;
      int         Int_Comp;
      char        Str_Comp[31];
    } var_1;
    struct {
      Enumeration E_Comp_2;
      char        Str_2_Comp[31];
    } var_2;
    struct {
      char Ch_1_Comp;
      char Ch_2_Comp;
    } var_3;
  } variant;
} Rec_Type, *Rec_Pointer;

#define Null  0
#define true  1
#define false 0

/* ---- dhry_1.c / dhry_2.c: globals and functions ---- */

static Rec_Pointer Ptr_Glob, Next_Ptr_Glob;
static int         Int_Glob;
static Boolean     Bool_Glob;
static char        Ch_1_Glob, Ch_2_Glob;
static int         Arr_1_Glob[50];
static int         Arr_2_Glob[50][50];

static Enumeration Func_1(Capital_Letter Ch_1_Par_Val, Capital_Letter Ch_2_Par_Val);
static Boolean     Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref);
static Boolean     Func_3(Enumeration Enum_Par_Val);
static void        Proc_1(Rec_Pointer Ptr_Val_Par);
static void        Proc_2(One_Fifty *Int_Par_Ref);
static void        Proc_3(Rec_Pointer *Ptr_Ref_Par);
static void        Proc_4(void);
static void        Proc_5(void);
static void        Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par);
static void        Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val, One_Fifty *Int_Par_Ref);
static void        Proc_8(Arr_1_Dim Arr_1_Par_Ref, Arr_2_Dim Arr_2_Par_Ref, int Int_1_Par_Val, int Int_2_Par_Val);

static void dhry(int Number_Of_Runs)
{
  One_Fifty   Int_1_Loc;
  One_Fifty   Int_2_Loc;
  One_Fifty   Int_3_Loc;
  char        Ch_Index;
  Enumeration Enum_Loc;
  Str_30      Str_1_Loc;
  Str_30      Str_2_Loc;
  int         Run_Index;

  Next_Ptr_Glob = (Rec_Pointer) malloc(sizeof(Rec_Type));
  Ptr_Glob      = (Rec_Pointer) malloc(sizeof(Rec_Type));

  Ptr_Glob->Ptr_Comp                = Next_Ptr_Glob;
  Ptr_Glob->Discr                   = Ident_1;
  Ptr_Glob->variant.var_1.Enum_Comp = Ident_3;
  Ptr_Glob->variant.var_1.Int_Comp  = 40;
  strcpy(Ptr_Glob->variant.var_1.Str_Comp, "DHRYSTONE PROGRAM, SOME STRING");
  strcpy(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING");

  /* Was missing in the originally published program - without this,
   * Arr_2_Glob[8][7] would have an undefined value. */
  Arr_2_Glob[8][7] = 10;

  for (Run_Index = 1; Run_Index < Number_Of_Runs; ++Run_Index) {
    Proc_5();
    Proc_4();
    /* Ch_1_Glob == 'A', Ch_2_Glob == 'B', Bool_Glob == true */
    Int_1_Loc = 2;
    Int_2_Loc = 3;
    strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING");
    Enum_Loc = Ident_2;
    Bool_Glob = !Func_2(Str_1_Loc, Str_2_Loc);
    /* Bool_Glob == 1 */
    while (Int_1_Loc < Int_2_Loc) { /* loop body executed once */
      Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
      /* Int_3_Loc == 7 */
      Proc_7(Int_1_Loc, Int_2_Loc, &Int_3_Loc);
      /* Int_3_Loc == 7 */
      Int_1_Loc += 1;
    }
    /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
    Proc_8(Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
    /* Int_Glob == 5 */
    Proc_1(Ptr_Glob);
    for (Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index) { /* loop body executed twice */
      if (Enum_Loc == Func_1(Ch_Index, 'C')) { /* then, not executed */
        Proc_6(Ident_1, &Enum_Loc);
        strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING");
        Int_2_Loc = Run_Index;
        Int_Glob = Run_Index;
      }
    }
    /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
    Int_2_Loc = Int_2_Loc * Int_1_Loc;
    Int_1_Loc = Int_2_Loc / Int_3_Loc;
    Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
    /* Int_1_Loc == 1, Int_2_Loc == 13, Int_3_Loc == 7 */
    Proc_2(&Int_1_Loc);
    /* Int_1_Loc == 5 */
  }

  free(Ptr_Glob);
  free(Next_Ptr_Glob);
}

static void Proc_1(Rec_Pointer Ptr_Val_Par) /* executed once */
{
  Rec_Pointer Next_Record = Ptr_Val_Par->Ptr_Comp; /* == Ptr_Glob_Next */

  *Next_Record = *Ptr_Glob;
  Ptr_Val_Par->variant.var_1.Int_Comp = 5;
  Next_Record->variant.var_1.Int_Comp = Ptr_Val_Par->variant.var_1.Int_Comp;
  Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
  Proc_3(&Next_Record->Ptr_Comp);
  /* Ptr_Val_Par->Ptr_Comp->Ptr_Comp == Ptr_Glob->Ptr_Comp */
  if (Next_Record->Discr == Ident_1) { /* then, executed */
    Next_Record->variant.var_1.Int_Comp = 6;
    Proc_6(Ptr_Val_Par->variant.var_1.Enum_Comp,
           &Next_Record->variant.var_1.Enum_Comp);
    Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
    Proc_7(Next_Record->variant.var_1.Int_Comp, 10,
           &Next_Record->variant.var_1.Int_Comp);
  } else { /* not executed */
    *Ptr_Val_Par = *Ptr_Val_Par->Ptr_Comp;
  }
}

static void Proc_2(One_Fifty *Int_Par_Ref) /* executed once */
{
  One_Fifty   Int_Loc;
  Enumeration Enum_Loc = Ident_1;

  Int_Loc = *Int_Par_Ref + 10;
  do { /* executed once */
    if (Ch_1_Glob == 'A') { /* then, executed */
      Int_Loc -= 1;
      *Int_Par_Ref = Int_Loc - Int_Glob;
      Enum_Loc = Ident_1;
    }
  } while (Enum_Loc != Ident_1); /* true */
}

static void Proc_3(Rec_Pointer *Ptr_Ref_Par) /* executed once */
{
  if (Ptr_Glob != NULL) { /* then, executed */
    *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
  }
  Proc_7(10, Int_Glob, &Ptr_Glob->variant.var_1.Int_Comp);
}

static void Proc_4(void) /* executed once */
{
  Boolean Bool_Loc;

  Bool_Loc = Ch_1_Glob == 'A';
  Bool_Glob = Bool_Loc | Bool_Glob;
  Ch_2_Glob = 'B';
}

static void Proc_5(void) /* executed once */
{
  Ch_1_Glob = 'A';
  Bool_Glob = false;
}

static void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par) /* executed once */
{
  *Enum_Ref_Par = Enum_Val_Par;
  if (!Func_3(Enum_Val_Par)) { /* then, not executed */
    *Enum_Ref_Par = Ident_4;
  }
  switch (Enum_Val_Par) {
    case Ident_1:
      *Enum_Ref_Par = Ident_1;
      break;
    case Ident_2:
      if (Int_Glob > 100) {
        *Enum_Ref_Par = Ident_1;
      } else {
        *Enum_Ref_Par = Ident_4;
      }
      break;
    case Ident_3: /* executed */
      *Enum_Ref_Par = Ident_2;
      break;
    case Ident_4:
      break;
    case Ident_5:
      *Enum_Ref_Par = Ident_3;
      break;
  }
}

static void Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val,
                    One_Fifty *Int_Par_Ref) /* executed three times */
{
  One_Fifty Int_Loc;

  Int_Loc = Int_1_Par_Val + 2;
  *Int_Par_Ref = Int_2_Par_Val + Int_Loc;
}

static void Proc_8(Arr_1_Dim Arr_1_Par_Ref, Arr_2_Dim Arr_2_Par_Ref,
                    int Int_1_Par_Val, int Int_2_Par_Val) /* executed once */
{
  One_Fifty Int_Index, Int_Loc;

  Int_Loc = Int_1_Par_Val + 5;
  Arr_1_Par_Ref[Int_Loc] = Int_2_Par_Val;
  Arr_1_Par_Ref[Int_Loc + 1] = Arr_1_Par_Ref[Int_Loc];
  Arr_1_Par_Ref[Int_Loc + 30] = Int_Loc;
  for (Int_Index = Int_Loc; Int_Index <= Int_Loc + 1; ++Int_Index) {
    Arr_2_Par_Ref[Int_Loc][Int_Index] = Int_Loc;
  }
  Arr_2_Par_Ref[Int_Loc][Int_Loc - 1] += 1;
  Arr_2_Par_Ref[Int_Loc + 20][Int_Loc] = Arr_1_Par_Ref[Int_Loc];
  Int_Glob = 5;
}

static Enumeration Func_1(Capital_Letter Ch_1_Par_Val,
                           Capital_Letter Ch_2_Par_Val) /* executed three times */
{
  Capital_Letter Ch_1_Loc;
  Capital_Letter Ch_2_Loc;

  Ch_1_Loc = Ch_1_Par_Val;
  Ch_2_Loc = Ch_1_Loc;
  if (Ch_2_Loc != Ch_2_Par_Val) { /* then, executed */
    return Ident_1;
  } else { /* not executed */
    Ch_1_Glob = Ch_1_Loc;
    return Ident_2;
  }
}

static Boolean Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref) /* executed once */
{
  One_Thirty     Int_Loc;
  Capital_Letter Ch_Loc = 'A';

  Int_Loc = 2;
  while (Int_Loc <= 2) { /* loop body executed once */
    if (Func_1(Str_1_Par_Ref[Int_Loc], Str_2_Par_Ref[Int_Loc + 1]) == Ident_1) {
      /* then, executed */
      Ch_Loc = 'A';
      Int_Loc += 1;
    }
  }
  if (Ch_Loc >= 'W' && Ch_Loc < 'Z') { /* then, not executed */
    Int_Loc = 7;
  }
  if (Ch_Loc == 'R') { /* then, not executed */
    return true;
  } else { /* executed */
    if (strcmp(Str_1_Par_Ref, Str_2_Par_Ref) > 0) { /* then, not executed */
      Int_Loc += 7;
      Int_Glob = Int_Loc;
      return true;
    } else { /* executed */
      return false;
    }
  }
}

static Boolean Func_3(Enumeration Enum_Par_Val) /* executed once */
{
  Enumeration Enum_Loc = Enum_Par_Val;

  if (Enum_Loc == Ident_3) { /* then, executed */
    return true;
  } else { /* not executed */
    return false;
  }
}

/* ---- RTEMS harness: calibrate, time, and report over the console ---- */

#define CALIBRATE_MIN_SEC 2.0
#define DHRYSTONES_PER_VAX_MIPS 1757.0 /* VAX 11/780 reference, industry standard since Dhrystone's 1984 publication */

rtems_task Init(rtems_task_argument ignored)
{
  int      number_of_runs;
  uint64_t start_ns, end_ns;
  double   elapsed_sec;
  double   dhrystones_per_sec;
  double   dmips;

  (void) ignored;

  printf("\n");
  printf("Dhrystone 2.1 DMIPS benchmark - RTEMS on ESP32-C3 (esp32c3db)\n");
  printf("===============================================================\n");

  number_of_runs = 50000;
  for (;;) {
    start_ns = rtems_clock_get_uptime_nanoseconds();
    dhry(number_of_runs);
    end_ns = rtems_clock_get_uptime_nanoseconds();

    elapsed_sec = (double) (end_ns - start_ns) / 1e9;
    printf("  calibration: %d runs took %.3f s\n", number_of_runs, elapsed_sec);

    if (elapsed_sec >= CALIBRATE_MIN_SEC || number_of_runs > (INT_MAX / 4)) {
      break;
    }

    number_of_runs *= 4;
  }

  dhrystones_per_sec = (double) number_of_runs / elapsed_sec;
  dmips = dhrystones_per_sec / DHRYSTONES_PER_VAX_MIPS;

  printf("\n");
  printf("Runs:               %d\n", number_of_runs);
  printf("Elapsed time:        %.3f s\n", elapsed_sec);
  printf("Dhrystones/second:   %.1f\n", dhrystones_per_sec);
  printf("DMIPS:               %.2f\n", dmips);
  printf("(sanity check - benchmark globals after run: Int_Glob=%d "
         "Bool_Glob=%d Ch_1_Glob=%c Ch_2_Glob=%c)\n",
         Int_Glob, Bool_Glob, Ch_1_Glob, Ch_2_Glob);

  exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
