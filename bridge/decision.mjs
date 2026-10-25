export function validateState(state) {
  if (state?.version !== 1 || state.map !== 'de_dust2' ||
      !Number.isSafeInteger(state.request) || !Number.isSafeInteger(state.nonce) ||
      !Array.isArray(state.bots) || state.bots.length !== 5) throw new Error('invalid state');
  const ids = state.bots.map(b => b.id);
  if (new Set(ids).size !== 5 || ids.some(id => !Number.isInteger(id) || id < 1 || id > 32)) throw new Error('invalid roster');
  if (state.bots.some(b => !Array.isArray(b.origin) || b.origin.length !== 3 ||
      b.origin.some(v => !Number.isFinite(v)) || !Number.isFinite(b.health))) throw new Error('invalid bot');
  return state;
}

export function plansFor(state) {
  validateState(state);
  const ids = state.bots.map(b => b.id).sort((a,b) => a-b);
  const plans = {};
  for (let i=0; i<3; i++) for (let j=i+1; j<4; j++) for (let k=j+1; k<5; k++) {
    const a = [ids[i], ids[j], ids[k]];
    plans[`a_${a.join('_')}`] = { a, b: ids.filter(id => !a.includes(id)) };
  }
  return plans;
}

export function requestFor(state, model='typesafe/jev-1.14') {
  const plans = plansFor(state);
  return { model, state, questions: { deployment: {
    type: 'choice',
    instructions: 'Choose the Counter-Strike CT opening deployment that minimizes travel from spawn and maintains useful coverage. Exactly three bots defend A and two defend B. Use only the supplied state. Positions are GoldSrc world coordinates.',
    criteria: Object.fromEntries(Object.entries(plans).map(([key,p]) =>
      [key, `Bots ${p.a.join(', ')} go to A; bots ${p.b.join(', ')} go to B.`]))
  } } };
}

export function decodeAnswer(state, body) {
  const answer = body?.answers?.deployment;
  const plan = plansFor(state)[answer?.choice];
  if (answer?.type !== 'choice' || !plan) throw new Error('invalid model answer');
  return { plan, choice: answer.choice, confidence: answer.confidence ?? null,
    probabilities: answer.probabilities ?? null, model: body.model ?? null, usage: body.usage ?? null };
}

export function wireReply(state, plan) {
  const valid = Object.values(plansFor(state)).some(p => JSON.stringify(p) === JSON.stringify(plan));
  if (!valid) throw new Error('invalid split');
  return `YJ1 ${state.request} ${state.nonce} ${plan.a.join(' ')}`;
}

export async function decide(state, {apiKey, fetchFn=fetch, timeout=1800}={}) {
  if (!apiKey) throw new Error('missing OpenRouter key');
  const response = await fetchFn('https://openrouter.ai/api/alpha/decisions', {
    method: 'POST', headers: {Authorization: `Bearer ${apiKey}`, 'Content-Type':'application/json',
      'X-Title':'YaPB Jev Local MVP'}, body: JSON.stringify(requestFor(state)),
    signal: AbortSignal.timeout(timeout)
  });
  if (!response.ok) throw new Error(`OpenRouter HTTP ${response.status}`);
  return decodeAnswer(state, await response.json());
}
