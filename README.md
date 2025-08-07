# QapDNA(QapDecideNextAction)
## V1
```cpp
t_cmd decide_next_action(
  t_visible_part_of_world vpow,
  vector<t_cmd> allowed_actions
){
  static vector<t_cmd_with_vpow> log;
  t_reconstucted_world&w=log_and_vpow2rw(log,vpow);
  t_best_score best;
  for(auto&ex:allowed_actions){
    auto tmp=w;
    tmp.use(ex);
    tmp.step();
    best.apply_if_better({tmp.get_score(),ex});
  }
  log+={best.cmd,vpow};
  return best.cmd;
}
```
---
## with_attention_mechanism
```cpp
t_cmd decide_next_action(
  const t_visible_part_of_world&vpow,
  const vector<t_cmd>&allowed_cmds
){
  static vector<t_cmd_with_vpow> log;
  t_reconstructed_world&w=reconstruct_world(log,vpow);
  t_attention_mechanism&am=w.am;
  am.update_goals(w.impl)
  t_plan_generator&pg=w.pg;
  pg.use(am.cur_goal);
  t_executor&exe=w.exe;//brian?
  exe.use(pg.cur_cmd,allowed_cmds);
  t_best_score best;
  for(const auto&cmd:exe.cmds){
    auto tmp_w=w;
    tmp_w.impl.step(cmd);
    best.try_update({tmp_w.impl.get_score(),cmd});
  }
  log.emplace_back(best.cmd,vpow);
  return best.cmd;
}
```
---
## archive_write_loop
```cpp
while(is_archive_write_loop(am.cur_goal)){
  pg.use(am.cur_goal);
  msg=exe.to_text(pg.cur_cmd,w.impl.me.archive);
  w.impl.me.archive.add(msg); // me or my?
  ai_response=w.impl.me.with_scores(w.impl.ai[qwen].analyze(msg));
  if(w.impl.me.is_good(ai_response.ideas.best)){ 
    w.impl.me.focus(ai_response.ideas.best);
  }
  if(auto*p=w.impl.external_interrupt()){
    am.update(p);
  }
}
```
