%include {#include <assert.h>}
%include {#include <stdlib.h>}
%include {#include <string.h>}
%include {#include "job.h"}
%include {#include "utarray.h"}
%token_prefix TOK_
%token_type {char*}
%extra_argument {parse_t *ps}
%syntax_error  {
  utstring_printf(ps->em, "error in %s line %d ", ps->file, ps->line);
  ps->rc=-1;
}
%parse_failure {ps->rc=-1;}
%type path {char*}
%type arg {char*}

file ::= jobs.
file ::= .
jobs ::= jobs job.
jobs ::= job.
job ::= JOB LCURLY sbody RCURLY. {push_job(ps);}
sbody ::= sbody kv.
sbody ::= kv.
kv ::= NAME STR(A).    {set_name(ps,A);}
kv ::= CMD cmd.                         
kv ::= DIR path(A).    {set_dir(ps,A);}
kv ::= OUT path(A).    {set_out(ps,A);}
kv ::= ERR path(A).    {set_err(ps,A);}
kv ::= USER STR(A).    {set_usr(ps,A);}
kv ::= ORDER STR(A).   {set_ord(ps,A);}
kv ::= ENV STR(A).     {set_env(ps,A);}
kv ::= DISABLED.       {set_dis(ps);  }
kv ::= WAIT.           {set_wait(ps); }
kv ::= ONCE.           {set_once(ps); }
cmd ::= path(A).       {set_cmd(ps,A);}
cmd ::= path(A) args.  {set_cmd(ps,A);}
path(A) ::= STR(B).    {A=B;}
args ::= args arg(B).  {utarray_push_back(&ps->job->cmdv,&B);}
args ::= arg(B).       {utarray_push_back(&ps->job->cmdv,&B);}
arg(A) ::= STR(B).       {A=B;}
arg(A) ::= QUOTEDSTR(B). {A=unquote(B);}
