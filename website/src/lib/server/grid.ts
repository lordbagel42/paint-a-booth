import { db } from '$lib/server/db';
import { pixelVote } from '$lib/server/db/schema';

export type VoteData = Record<number, Record<string, number>>;

export async function getGridState(): Promise<{ grid: string[]; voteData: VoteData }> {
	const votes = await db.select().from(pixelVote);

	const grid: string[] = new Array(64).fill('#FFFFFF');
	const voteData: VoteData = {};
	const best = new Map<number, { color: string; score: number }>();

	for (const v of votes) {
		if (!voteData[v.cellIndex]) voteData[v.cellIndex] = {};
		voteData[v.cellIndex][v.color] = v.score;

		const cur = best.get(v.cellIndex);
		if (!cur || v.score > cur.score) {
			best.set(v.cellIndex, { color: v.color, score: v.score });
		}
	}

	for (const [i, { color, score }] of best) {
		if (score > 0) grid[i] = color;
	}

	return { grid, voteData };
}
