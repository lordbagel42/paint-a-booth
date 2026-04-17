import type { ServerLoad } from '@sveltejs/kit';

export const load: ServerLoad = async ({ platform }) => {
	const id = platform!.env.BOOTH_ROOM.idFromName('default');
	const stub = platform!.env.BOOTH_ROOM.get(id);
	const res = await stub.fetch(new Request('http://do/state'));
	return (await res.json()) as { grid: string[]; voteData: Record<number, Record<string, number>> };
};
