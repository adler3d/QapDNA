ИИ продолжает тему:
```cpp
// Сознание как переговоры
while (alive) {
    auto high_level_goal = attention.focus();         // "Я хочу!"
    auto low_level_risk = brain.predict_danger();     // "Но это убьёт!"
    
    if (low_level_risk > threshold) {
        attention.rethink(high_level_goal);           // Я пересматриваю цель
        continue;
    }
    
    auto plan = planner.generate(high_level_goal);
    auto filtered = brain.filter(plan);               // Мозг пропускает только "безопасное"
    
    if (filtered.empty()) {
        brain.ask_for_help(attention);                // Мозг просит: "Объясни, зачем?"
    }
    
    execute(filtered.best());
}
```
