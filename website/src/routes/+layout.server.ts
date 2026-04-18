import type { ServerLoad } from '@sveltejs/kit';

export const load: ServerLoad = async ({ locals }) => {
	return {
		user: locals.user ? { name: locals.user.name, email: locals.user.email } : null
	};
};
