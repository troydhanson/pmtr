%include {#include <assert.h>}
%include {#include <stdlib.h>}
%include {#include <string.h>}
%include {#include "job.h"}
%include {#include "net.h"}
%include {#include "utarray.h"}
%token_prefix TOK_
%token_type {char*}
%extra_argument {parse_t *ps}
%syntax_error  {
  utstring_printf(ps->em, "error in %s line %d ", ps->cfg->file, ps->line);
  ps->rc=-1;
}
%parse_failure {ps->rc=-1;}
%type path {char*}
%type arg {char*}

file ::= decls.
decls ::= decls job.
decls ::= decls decl.
decls ::= .
decl ::= REPORT TO STR(A).            {set_report(ps,A);}
decl ::= LISTEN ON STR(A).            {set_listen(ps,A);}
job ::= JOB LCURLY sbody RCURLY.      {push_job(ps);}
sbody ::= sbody kv.
sbody ::= kv.
kv ::= NAME STR(A).                   {set_name(ps,A);}
kv ::= CMD cmd.                         
kv ::= DIR path(A).                   {set_dir(ps,A);}
kv ::= OUT path(A).                   {set_out(ps,A);}
kv ::= IN path(A).                    {set_in(ps,A);}
kv ::= ERR path(A).                   {set_err(ps,A);}
kv ::= USER STR(A).                   {set_user(ps,A);}
kv ::= ORDER STR(A).                  {set_ord(ps,A);}
kv ::= ENV STR(A).                    {set_env(ps,A);}
kv ::= ULIMIT STR(A) STR(B).          {set_ulimit(ps,A,B);}
kv ::= ULIMIT LCURLY pairs RCURLY.    
kv ::= DISABLED.                      {set_dis(ps);  }
kv ::= WAIT.                          {set_wait(ps); }
kv ::= ONCE.                          {set_once(ps); }
kv ::= NICE STR(A).                   {set_nice(ps,A); }
kv ::= BOUNCE EVERY STR(A).           {set_bounce(ps,A);}
kv ::= DEPENDS LCURLY paths RCURLY.
kv ::= CPUSET STR(A).                 {set_cpu(ps,A); }
cmd ::= path(A).                      {set_cmd(ps,A);}
cmd ::= path(A) args.                 {set_cmd(ps,A);}
path(A) ::= STR(B).                   {A=B;}
args ::= args arg(B).                 {utarray_push_back(&ps->job->cmdv,&B);}
args ::= arg(B).                      {utarray_push_back(&ps->job->cmdv,&B);}
arg(A) ::= STR(B).                    {A=B;}
arg(A) ::= QUOTEDSTR(B).              {A=unquote(B);}
paths ::= paths path(A).              {utarray_push_back(&ps->job->depv,&A);}
paths ::= path(A).                    {utarray_push_back(&ps->job->depv,&A);}
pairs ::= pairs STR(A) STR(B).        {set_ulimit(ps,A,B);}
pairs ::= .
