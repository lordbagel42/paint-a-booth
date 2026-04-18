import type { ServerLoad } from '@sveltejs/kit';

export const load: ServerLoad = async ({ platform, locals }) => {
	const id = platform!.env.BOOTH_ROOM.idFromName('default');
	const stub = platform!.env.BOOTH_ROOM.get(id);
	const res = await stub.fetch(new Request('http://do/state'));
	const state = (await res.json()) as {
		grid: string[];
		voteData: Record<number, Record<string, number>>;
	};
	return {
		...state,
		user: locals.user ? { id: locals.user.id, name: locals.user.name } : null
	};
};
